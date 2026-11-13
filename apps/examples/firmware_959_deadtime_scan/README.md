# Firmware 959 deadtime scan

This application compares normal and WLC deadtime without MIDAS. A Python scan driver controls the Pico double-pulse generator. For each measurement, the C++ application starts a UDP listener thread, configures the board, captures for a fixed interval, and reports packet/event counters.

The scan appends and `fsync`s one JSON object after every measured separation. Rerunning the same command reads the existing JSONL file, reconstructs each binary-search interval, and continues incomplete combinations. The `parameter_space` contains independent lists for `windows`, `pulse_rates_hz`, `wlc_enabled`, and channel sets; the scan runs their Cartesian product.

Measurements with zero parsed events are written with `measurement_status: invalid_zero_events`. Ratios above `binary_search.maximum_valid_ratio` are written as `invalid_implausible_ratio`. Both are retried without advancing the binary search. `run.zero_event_max_retries` controls the number of additional attempts. If retries are exhausted, only that separation is marked `failed_after_retries`; it is treated conservatively as a failed deadtime point and the same parameter search continues. Individual invalid attempts are ignored when resuming, while the final failed search point is retained.

`binary_search.start_ns` is measured first for each parameter combination before ordinary binary-search midpoints. The focused 200–1000 ns configuration starts at 240 ns.

`windows` means the desired returned record length in both modes. Normal mode requests it directly. In WLC mode the controller converts it to an ASIC request using the documented `2*ceil(n/2)+2` behavior. WLC therefore accepts only even returned lengths from 4 through 14 and rejects unrepresentable values.

The board process remains alive for the scan. When a binary-search measurement repeats the same WLC mode, window request, and channel set, it restarts readout without reprogramming those settings. Changed settings are applied at the next measurement.

The deadtime pass metric matches the earlier MIDAS scans: observed packet rate is divided by the expected **single-pulse** packet rate. A resolved double pulse therefore has a ratio near 2, and the default pass threshold is 1.9.

The default trigger mode is `self`; connect the Pico double-pulse output to the configured signal input and adjust `active_channels` if necessary.

The default channel parameter space contains cumulative sets of 1 through 13 working channels from `[0,1,2,3,4,5,6,8,9,10,11,12,13]`; channel 7 is excluded.

```bash
./apps/examples/firmware_959_deadtime_scan/scripts/build.sh
./apps/examples/firmware_959_deadtime_scan/scripts/run.sh --dry-run
./apps/examples/firmware_959_deadtime_scan/scripts/run.sh
```

Run detached with GNU Screen:

```bash
./scripts/run.sh firmware_959_deadtime_scan --screen
./scripts/run.sh firmware_959_deadtime_scan --screen-status
screen -r firmware_959_deadtime_scan
./scripts/run.sh firmware_959_deadtime_scan --screen-stop
```

The live output reports the current parameter combination, binary-search iteration, completed measurements, and estimated time remaining. Screen output is also written under `rate_data/`.

Results default to `rate_data/firmware_959_wlc_deadtime.jsonl` inside this application.
