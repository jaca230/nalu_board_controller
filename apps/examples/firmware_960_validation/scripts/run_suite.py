#!/usr/bin/env python3
"""Run self-trigger tests with the Pico active, then external mode with it stopped."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
APP_DIR = SCRIPT_DIR.parent
EXECUTABLE = APP_DIR / "build/bin/firmware_960_validation"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=APP_DIR / "config.json")
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def run_checked(command: list[str]) -> None:
    subprocess.run(command, check=True)


def run_validation(config: Path, output: Path, scenarios: list[str]) -> int:
    command = [str(EXECUTABLE), "--config", str(config), "--output", str(output)]
    for scenario in scenarios:
        command.extend(("--scenario", scenario))
    return subprocess.run(command, check=False).returncode


def main() -> int:
    args = parse_args()
    config_path = args.config.resolve()
    document = json.loads(config_path.read_text())
    pico = document["pico"]
    pico_scripts = Path(pico["scripts_directory"])
    output = args.output or (config_path.parent / document["run"]["output"])
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    start_command = [
        sys.executable,
        str(pico_scripts / "start_single_pulse.py"),
        "--serial_port",
        pico["serial_port"],
        "--hardware_id",
        pico["hardware_id"],
        "--pin_number",
        str(pico["pin_number"]),
        "--repetition_rate_hz",
        str(pico["repetition_rate_hz"]),
        "--pulse_width_ns",
        str(pico["pulse_width_ns"]),
        "--state_machine_id",
        str(pico["state_machine_id"]),
        "--state_machine_freq",
        str(pico["state_machine_freq"]),
    ]
    stop_command = [
        sys.executable,
        str(pico_scripts / "stop_single_pulse.py"),
        "--serial_port",
        pico["serial_port"],
        "--hardware_id",
        pico["hardware_id"],
    ]

    with tempfile.TemporaryDirectory(prefix="firmware_960_validation_") as temporary:
        temporary_path = Path(temporary)
        self_output = temporary_path / "self.json"
        external_output = temporary_path / "external.json"
        self_returncode = 1
        try:
            run_checked(start_command)
            self_returncode = run_validation(
                config_path,
                self_output,
                ["normal_self_trigger", "wlc_self_trigger"],
            )
        finally:
            run_checked(stop_command)

        external_returncode = run_validation(
            config_path, external_output, ["external_trigger_without_source"]
        )
        self_report = (
            json.loads(self_output.read_text())
            if self_output.exists()
            else {
                "expected_firmware_substring": document["expected_firmware_substring"],
                "firmware_readback": None,
                "firmware_status": "failed",
                "scenarios": [],
                "failure": "self-trigger validation exited before writing a report",
            }
        )
        external_report = (
            json.loads(external_output.read_text())
            if external_output.exists()
            else {
                "scenarios": [],
                "failure": "external-trigger validation exited before writing a report",
            }
        )
        report = {
            "expected_firmware_substring": self_report["expected_firmware_substring"],
            "firmware_readback": self_report["firmware_readback"],
            "firmware_status": self_report["firmware_status"],
            "pico": pico,
            "scenarios": self_report["scenarios"] + external_report["scenarios"],
            "status": (
                "passed"
                if self_returncode == 0 and external_returncode == 0
                else "failed"
            ),
        }
        output.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
        return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
