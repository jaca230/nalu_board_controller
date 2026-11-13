#!/usr/bin/env python3
"""Firmware 959 single-pulse rate and capture-efficiency scan."""

import argparse
import itertools
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


APP_DIR = Path(__file__).resolve().parents[1]


def append_jsonl(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(value, separators=(",", ":")) + "\n")
        stream.flush()
        os.fsync(stream.fileno())


def read_records(path):
    records = []
    if not path.exists():
        return records
    for line in path.read_text(encoding="utf-8").splitlines():
        try:
            records.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return records


def resolve_app_path(value):
    path = Path(value)
    return path if path.is_absolute() else (APP_DIR / path).resolve()


def window_mapping(desired, wlc_enabled):
    if not wlc_enabled:
        return desired, desired
    requested = max(1, min(12, desired - 2))
    returned = 2 * ((requested + 1) // 2) + 2
    return requested, returned


def rate_points(parameter_space, returned_windows, channel_count):
    maximum_rate = int(parameter_space.get("maximum_pulse_rate_hz", 200_000))
    if "target_payload_percentages" in parameter_space:
        reference = float(parameter_space["reference_payload_bytes_per_second"])
        bytes_per_event = 74 * returned_windows * channel_count
        emitted_rates = set()
        for percentage in parameter_space["target_payload_percentages"]:
            target = reference * float(percentage) / 100.0
            requested_rate = max(1, int(round(target / bytes_per_event)))
            rate = min(requested_rate, maximum_rate)
            if rate in emitted_rates:
                continue
            emitted_rates.add(rate)
            yield {
                "rate_hz": rate,
                "requested_rate_hz": requested_rate,
                "rate_limited": requested_rate > maximum_rate,
                "target_payload_percentage": percentage,
                "target_payload_bytes_per_second": target,
                "commanded_payload_bytes_per_second": rate * bytes_per_event,
                "commanded_payload_percentage":
                    100.0 * rate * bytes_per_event / reference,
            }
        return

    for rate in parameter_space["pulse_rates_hz"]:
        if rate > maximum_rate:
            raise ValueError(
                f"Explicit pulse rate {rate} exceeds maximum_pulse_rate_hz={maximum_rate}"
            )
        yield {
            "rate_hz": rate,
            "requested_rate_hz": rate,
            "rate_limited": False,
            "target_payload_percentage": None,
            "target_payload_bytes_per_second": None,
            "commanded_payload_bytes_per_second": rate * 74 * returned_windows * channel_count,
            "commanded_payload_percentage": None,
        }


def format_duration(seconds):
    seconds = max(0, int(round(seconds)))
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours}h {minutes:02d}m"
    if minutes:
        return f"{minutes}m {seconds:02d}s"
    return f"{seconds}s"


class Pico:
    def __init__(self, config):
        sys.path.insert(0, str(Path(config["source_directory"])))
        from serial_command_sender import SerialCommandSender
        from serial_controller import SerialController

        self.config = config
        self.hardware_id = config["hardware_id"]
        self.serial = SerialController(port=config["serial_port"], debug=False)
        self.serial.connect()
        self.sender = SerialCommandSender(self.serial)
        self.created = False
        self.current_rate = None
        self.running = False

    def _safe_send(self, command, *args):
        try:
            return self.sender.send_command(command, *args)
        except Exception:
            return None

    def configure(self, rate):
        if not self.created:
            self._safe_send("stop", self.hardware_id)
            self._safe_send("delete", self.hardware_id)
            settings = {
                "pin_number": self.config["pin_number"],
                "repetition_rate_hz": rate,
                "pulse_width_ns": self.config["pulse_width_ns"],
                "state_machine_id": self.config.get("state_machine_id", 0),
                "state_machine_freq": self.config.get("state_machine_freq", 125_000_000),
                "start_on_init": False,
            }
            self.sender.send_command("create", "single_pulse", settings, self.hardware_id)
            self.created = True
            self.current_rate = rate
        elif self.current_rate != rate:
            self.sender.send_command(
                "apply_hardware_settings",
                self.hardware_id,
                {"repetition_rate_hz": rate},
            )
            self.current_rate = rate

        if not self.running:
            self.sender.send_command("start", self.hardware_id)
            self.running = True

    def stop(self):
        if self.running:
            self.sender.send_command("stop", self.hardware_id)
            self.running = False

    def close(self):
        try:
            self.stop()
        finally:
            self.serial.close()


class BoardProcess:
    def __init__(self, executable, config_path):
        self.process = subprocess.Popen(
            [str(executable), "--config", str(config_path), "--serve"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

    def measure(
        self,
        wlc,
        requested_windows,
        channels,
        duration,
        raw_output_path,
        force_reconfigure=False,
        capture_restart_warmup_seconds=0.0,
    ):
        request = {
            "wlc_enabled": wlc,
            "requested_windows": requested_windows,
            "active_channels": channels,
            "duration_seconds": duration,
            "raw_output_path": str(raw_output_path),
            "force_reconfigure": force_reconfigure,
            "capture_restart_warmup_seconds": capture_restart_warmup_seconds,
        }
        self.process.stdin.write(json.dumps(request) + "\n")
        self.process.stdin.flush()
        for line in self.process.stdout:
            if line.startswith("RESULT_JSON:"):
                return json.loads(line[len("RESULT_JSON:") :])
            print(line, end="")
        raise RuntimeError(f"Board process exited with {self.process.poll()}")

    def close(self):
        if self.process.poll() is not None:
            return
        try:
            if self.process.stdin:
                self.process.stdin.close()
            self.process.wait(timeout=5)
        except (subprocess.TimeoutExpired, BrokenPipeError):
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


def dry_result(requested, returned, channels, duration, rate):
    expected_pulses = rate * duration
    timestamp_summary = {
        "observed": int(expected_pulses),
        "typical_rate_hz": rate,
        "span_rate_hz": rate,
        "large_gap_count": 0,
        "inferred_missing": 0,
        "inferred_sent": int(expected_pulses),
        "gap_capture_efficiency": 1.0,
    }
    return {
        "requested_windows": requested,
        "active_channels": channels,
        "duration_seconds": duration,
        "capture_elapsed_seconds": duration,
        "measurement_start_offset_ns": 0,
        "measurement_end_offset_ns": int(duration * 1_000_000_000),
        "payload_bytes": int(expected_pulses * returned * len(channels) * 74),
        "packets_total": int(expected_pulses * returned * len(channels)),
        "trigger_groups_total": int(expected_pulses),
        "event_analysis": {
            "events_total": int(expected_pulses),
            "event_timestamps": timestamp_summary,
        },
        "packets_per_second": rate * returned * len(channels),
        "trigger_groups_per_second": rate,
        "channel_packets": {str(channel): int(expected_pulses * returned) for channel in channels},
        "channel_trigger_groups": {str(channel): int(expected_pulses) for channel in channels},
        "malformed_bytes": 0,
        "raw_output_path": "",
        "raw_file_bytes": 0,
        "dry_run": True,
    }


def measurement_metrics(point, rate, returned_windows, channel_count):
    elapsed = float(point.get("capture_elapsed_seconds", 0.0))
    expected_pulses = rate * elapsed
    expected_packets = expected_pulses * returned_windows * channel_count
    event_analysis = point.get("event_analysis", {})
    raw_sample_was_capped = point.get("raw_event_limit_reached", False)
    observed_events = (
        point.get("trigger_groups_total", 0)
        if raw_sample_was_capped
        else event_analysis.get("events_total", point.get("trigger_groups_total", 0))
    )
    timestamp_stats = event_analysis.get("event_timestamps", {})
    packets = point.get("packets_total", 0)
    return {
        "expected_pulses": expected_pulses,
        "expected_packets": expected_packets,
        "observed_events": observed_events,
        "observed_events_source": (
            "live_anchor_channel" if raw_sample_was_capped else "decoded_raw_events"
        ),
        "observed_trigger_groups_legacy": point.get("trigger_groups_total", 0),
        "observed_packets": packets,
        "trigger_capture_efficiency": observed_events / expected_pulses if expected_pulses else 0.0,
        "packet_capture_efficiency": packets / expected_packets if expected_packets else 0.0,
        "observed_events_per_second": observed_events / elapsed if elapsed else 0.0,
        "observed_packets_per_second": packets / elapsed if elapsed else 0.0,
        "observed_payload_bytes_per_second": point.get("payload_bytes", 0) / elapsed if elapsed else 0.0,
        "timestamp_typical_rate_hz": timestamp_stats.get("typical_rate_hz", 0.0),
        "timestamp_span_rate_hz": timestamp_stats.get("span_rate_hz", 0.0),
        "timestamp_large_gap_count": timestamp_stats.get("large_gap_count", 0),
        "timestamp_inferred_missing": timestamp_stats.get("inferred_missing", 0),
        "timestamp_gap_capture_efficiency": timestamp_stats.get(
            "gap_capture_efficiency", 0.0
        ),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=APP_DIR / "config.json")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    config_path = resolve_app_path(args.config)
    config = json.loads(config_path.read_text(encoding="utf-8"))
    executable = APP_DIR / "build/bin/firmware_959_rate_point"
    if not args.dry_run and not executable.exists():
        raise SystemExit(f"Build first: {APP_DIR}/scripts/build.sh")

    output = resolve_app_path(config["logging"]["output"])
    raw_directory = resolve_app_path(config["logging"]["raw_directory"])
    raw_directory.mkdir(parents=True, exist_ok=True)
    existing = read_records(output)

    run = config["run"]
    space = config["parameter_space"]
    combinations = []
    for wlc, channel_set, desired_windows, sample_index in itertools.product(
        space["wlc_enabled"],
        space["channels"],
        space["windows"],
        range(int(run.get("samples_per_point", 1))),
    ):
        requested, returned = window_mapping(desired_windows, wlc)
        for rate_point in rate_points(space, returned, len(channel_set["channels"])):
            combinations.append(
                (
                    wlc,
                    channel_set,
                    desired_windows,
                    requested,
                    returned,
                    rate_point,
                    sample_index,
                )
            )
    maximum_rate = max(item[5]["rate_hz"] for item in combinations)
    pulse_width_ns = float(config["pico"]["pulse_width_ns"])
    if pulse_width_ns >= 1_000_000_000 / maximum_rate:
        raise SystemExit(
            f"Computed maximum rate {maximum_rate} Hz is incompatible with "
            f"pulse_width_ns={pulse_width_ns:g}"
        )
    completed_keys = {
        json.dumps(record.get("scan_key"), sort_keys=True)
        for record in existing
        if record.get("measurement_status") in {"valid", "failed_after_retries"}
    }
    pending = []
    for wlc, channel_set, desired_windows, requested, returned, rate_point, sample_index in combinations:
        rate = rate_point["rate_hz"]
        key = {
            "rate_hz": rate,
            "rate_limited": rate_point["rate_limited"],
            "target_payload_percentage": rate_point["target_payload_percentage"],
            "reference_payload_bytes_per_second": space.get(
                "reference_payload_bytes_per_second"
            ),
            "desired_windows": desired_windows,
            "returned_windows": returned,
            "mode": "wlc" if wlc else "normal",
            "channel_set": channel_set["id"],
            "sample_index": sample_index,
            "metric": "single_pulse_event_efficiency_v6",
        }
        if json.dumps(key, sort_keys=True) not in completed_keys:
            pending.append(
                (
                    wlc,
                    channel_set,
                    desired_windows,
                    requested,
                    returned,
                    rate_point,
                    sample_index,
                    key,
                )
            )

    pico = None if args.dry_run else Pico(config["pico"])
    board = None if args.dry_run else BoardProcess(executable, config_path)
    measured = 0
    measurement_seconds = 0.0
    max_retries = int(run.get("zero_event_max_retries", 0))
    maximum_efficiency = float(run.get("maximum_valid_efficiency", 1.2))
    force_reconfigure_next_measurement = False

    try:
        for index, item in enumerate(pending, start=1):
            wlc, channel_set, desired, requested, returned, rate_point, sample_index, key = item
            rate = rate_point["rate_hz"]
            channels = channel_set["channels"]
            point_started = time.monotonic()
            average = measurement_seconds / measured if measured else (
                run["duration_seconds"] + run["settle_seconds"] + 4
            )
            target_label = (
                f"{rate_point['target_payload_percentage']}%"
                if rate_point["target_payload_percentage"] is not None
                else "explicit"
            )
            print(
                f"[point {index}/{len(pending)}] starting rate={rate:g} Hz "
                f"target={target_label} "
                f"mode={key['mode']} channels={channel_set['id']} windows={desired} "
                f"sample={sample_index + 1}; ETA {format_duration((len(pending)-index+1)*average)}",
                flush=True,
            )

            retry_count = 0
            while True:
                timestamp = datetime.now(timezone.utc)
                filename = (
                    f"rate_{rate:g}_hz_{key['mode']}_{channel_set['id']}_w{returned}_"
                    f"sample_{sample_index}_attempt_{retry_count}_{timestamp.strftime('%Y%m%dT%H%M%S%fZ')}.udpbin"
                )
                raw_path = raw_directory / filename

                if pico:
                    pico.configure(rate)
                    time.sleep(float(run["settle_seconds"]))
                try:
                    point = (
                        dry_result(requested, returned, channels, run["duration_seconds"], rate)
                        if args.dry_run
                        else board.measure(
                            wlc,
                            requested,
                            channels,
                            run["duration_seconds"],
                            raw_path,
                            force_reconfigure=force_reconfigure_next_measurement,
                            capture_restart_warmup_seconds=float(
                                run.get("capture_restart_warmup_seconds", 0.0)
                            ),
                        )
                    )
                    force_reconfigure_next_measurement = False
                finally:
                    if pico:
                        pico.stop()

                metrics = measurement_metrics(point, rate, returned, len(channels))
                invalid_reason = None
                observed_events = metrics["observed_events"]
                if not args.dry_run and observed_events == 0:
                    invalid_reason = "zero_events"
                elif not args.dry_run and (
                    metrics["trigger_capture_efficiency"] > maximum_efficiency
                    or metrics["packet_capture_efficiency"] > maximum_efficiency
                ):
                    invalid_reason = "implausible_efficiency"

                if invalid_reason is not None:
                    force_reconfigure_next_measurement = True

                base_record = {
                    "timestamp": timestamp.isoformat(),
                    "scan_key": key,
                    "single_pulse": {
                        "repetition_rate_hz": rate,
                        "requested_repetition_rate_hz": rate_point["requested_rate_hz"],
                        "rate_limited": rate_point["rate_limited"],
                        "pulse_width_ns": config["pico"]["pulse_width_ns"],
                        "state_machine_freq": config["pico"].get("state_machine_freq", 125_000_000),
                        "target_payload_percentage": rate_point["target_payload_percentage"],
                        "target_payload_bytes_per_second": rate_point[
                            "target_payload_bytes_per_second"
                        ],
                        "commanded_payload_bytes_per_second": rate_point[
                            "commanded_payload_bytes_per_second"
                        ],
                        "commanded_payload_percentage": rate_point[
                            "commanded_payload_percentage"
                        ],
                    },
                    "capture_settings": {
                        "mode": key["mode"],
                        "wlc_enabled": wlc,
                        "desired_windows": desired,
                        "requested_windows": requested,
                        "expected_returned_windows": returned,
                        "active_channels": channels,
                        "channel_set": channel_set["id"],
                    },
                    "rates_and_efficiency": metrics,
                    "listener": point,
                    "raw_data": {
                        "path": point.get("raw_output_path", ""),
                        "bytes": point.get("raw_file_bytes", 0),
                        "format": "NALUUDP1",
                        "event_limit": point.get("raw_event_limit", 0),
                        "event_limit_reached": point.get("raw_event_limit_reached", False),
                    },
                    "retry_count": retry_count,
                    "config": config,
                }

                if invalid_reason is None:
                    base_record["measurement_status"] = "valid"
                    append_jsonl(output, base_record)
                    print(
                        f"rate={rate:g} Hz events/s={metrics['observed_events_per_second']:.1f} "
                        f"timestamp_rate={metrics['timestamp_typical_rate_hz']:.1f} Hz "
                        f"trigger_eff={metrics['trigger_capture_efficiency']:.4f} "
                        f"gap_eff={metrics['timestamp_gap_capture_efficiency']:.4f} "
                        f"inferred_missing={metrics['timestamp_inferred_missing']}",
                        flush=True,
                    )
                    break

                invalid_record = dict(base_record)
                invalid_record["measurement_status"] = f"invalid_{invalid_reason}"
                invalid_record["retry"] = {
                    "failed_attempt": retry_count + 1,
                    "max_retries": max_retries,
                    "exhausted": retry_count >= max_retries,
                }
                append_jsonl(output, invalid_record)

                if retry_count >= max_retries:
                    failed_record = dict(base_record)
                    failed_record["measurement_status"] = "failed_after_retries"
                    failed_record["failure_reason"] = invalid_reason
                    append_jsonl(output, failed_record)
                    print(
                        f"Point failed after {max_retries + 1} attempts ({invalid_reason})",
                        file=sys.stderr,
                        flush=True,
                    )
                    break

                retry_count += 1
                delay = float(run.get("zero_event_retry_delay_seconds", 0))
                print(
                    f"Invalid measurement ({invalid_reason}); retry {retry_count}/{max_retries} "
                    f"after {delay:g}s",
                    flush=True,
                )
                if delay:
                    time.sleep(delay)

            measured += 1
            measurement_seconds += time.monotonic() - point_started
            remaining = len(pending) - index
            print(
                f"Progress: {index}/{len(pending)}; "
                f"ETA {format_duration(remaining * measurement_seconds / measured)}",
                flush=True,
            )
    except KeyboardInterrupt:
        print("\nInterrupt received; stopping Pico and board capture...", file=sys.stderr)
    finally:
        if pico:
            pico.close()
        if board:
            board.close()

    print(f"Results: {output}")
    print(f"Raw data: {raw_directory}")


if __name__ == "__main__":
    main()
