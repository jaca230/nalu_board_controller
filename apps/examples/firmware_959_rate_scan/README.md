# Firmware 959 single-pulse rate scan

This application drives the deterministic Pico `single_pulse` generator through
an ordered payload-rate space and records a short raw UDP capture at every point. It is
intended to measure both output quantity (events, packets, and bytes per
second) and capture efficiency relative to the commanded Pico pulse rate.

Raw packets are decoded with `nalu_event_collector` and assembled as complete
per-channel window groups before groups are merged into events. Event and
per-channel timestamp sequences are analyzed with counter-wrap handling. A gap
at least 1.5 times the median timestamp step is treated as one or more inferred
missing pulses.

The scan retains the firmware-959 deadtime scan's operational behavior:

- the board process remains initialized between points;
- board capture stays active while only the Pico rate changes;
- capture is stopped and fully reconfigured when channels, windows, or WLC mode change;
- a zero-data attempt forces a full capture restart/reconfiguration before its retry;
- restart-only warm-up data is recorded but excluded from the measurement interval;
- the Pico is stopped between measurements;
- JSONL metadata is appended and `fsync`ed after every attempt;
- completed points are skipped when a scan is resumed;
- zero-event and implausible-efficiency attempts are retained and retried;
- exhausted points are marked `failed_after_retries` without stopping the scan;
- live progress, ETA, GNU Screen operation, and clean interrupt handling are included.

## Raw format

Each attempt writes one `.udpbin` file. The file begins with the eight-byte
ASCII magic `NALUUDP1`. Each received UDP datagram then has this framing:

| Field | Encoding |
|---|---|
| Receive time since listener start | unsigned 64-bit little-endian nanoseconds |
| Datagram length | unsigned 32-bit little-endian bytes |
| Datagram | the complete bytes received from the socket, including the configured UDP header |

This preserves datagram boundaries and timing for offline reprocessing. The
JSONL record stores the measurement start/end offsets into that timeline, raw
path, commanded rate, actual capture duration, capture settings, online
counters, and preliminary efficiency values.

Raw storage is capped by `raw_capture.max_events` (10,000 by default). The
receiver continues processing the full measurement after the cap, but stops
writing once the next UDP datagram would take the anchor-channel trigger count
past the limit. Consequently, the file contains at most 10,000 complete-event
candidates and can end with an incomplete boundary event, which the collector
decoder discards. `raw_event_limit_reached` records whether a point was capped.

`trigger_capture_efficiency` compares reconstructed events with commanded
pulses. `timestamp_gap_capture_efficiency` estimates losses from unusually
large timestamp jumps and `timestamp_typical_rate_hz` reports the cadence
derived from the median board-timestamp step. Packet-level counters remain in
the output as transport diagnostics; the raw files are authoritative for
offline reprocessing.

## Run

```bash
./apps/examples/firmware_959_rate_scan/scripts/build.sh
./apps/examples/firmware_959_rate_scan/scripts/run.sh --dry-run
./apps/examples/firmware_959_rate_scan/scripts/run.sh --config config.smoke.json --dry-run
./apps/examples/firmware_959_rate_scan/scripts/run.sh --config config.smoke.json
./apps/examples/firmware_959_rate_scan/scripts/run.sh
```

Detached operation:

```bash
./scripts/run.sh firmware_959_rate_scan --screen
screen -r firmware_959_rate_scan
./scripts/run.sh firmware_959_rate_scan --screen-stop
```

The full config targets 1–200% of 46.785 MB/s in 16 approximately logarithmic
steps. For each channel/window combination it computes
`rate = target_bytes_per_second / (74 * windows * channels)` and rounds to the
nearest hertz. Explicit `pulse_rates_hz` remains supported for smoke tests.
Generated rates have a hard 200 kHz ceiling. If multiple throughput targets
would exceed it, the scan emits one rate-limited 200 kHz boundary point rather
than repeating the same rate. The JSONL metadata records the requested rate,
actual commanded rate, and achieved payload percentage.
Edit `config.json` to set the payload targets, channel sets, windows, WLC modes,
measurement duration, retry policy, and output paths.

An existing raw point can be decoded again without board hardware:

```bash
./apps/examples/firmware_959_rate_scan/build/bin/firmware_959_rate_point \
  --config apps/examples/firmware_959_rate_scan/config.smoke.json \
  --analyze-raw PATH.udpbin --windows 8 --channels 0,1 \
  --measurement-start-ns START --measurement-end-ns END
```

The start/end offsets are stored in that point's JSONL `listener` object.
