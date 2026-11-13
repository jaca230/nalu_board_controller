# Firmware 960 acquisition validation

This short hardware test verifies the firmware readback and exercises the
acquisition paths needed before removing firmware-959 workarounds:

- normal self-trigger acquisition produces data;
- WLC self-trigger acquisition produces data;
- external-trigger acquisition with no source returns zero data and exits
  cleanly.

The self-trigger checks also validate event shape and multiplicity. Normal mode
expects the requested window count directly. In WLC mode, the public window
setting is also the desired returned count; the controller converts it to the
ASIC request using `returned = 2 * ceil(asic_request / 2) + 2`. Consequently,
WLC accepts the even returned lengths 4–14. For example, a user request of 8
is sent to the ASIC as 6 and must return 8 packets per channel-trigger. Both modes must also
produce one trigger group per 1 kHz Pico pulse within 5%; a historical 2×
trigger regression therefore fails the suite explicitly.

The full runner starts the deterministic Pico source at 1 kHz for the two
self-trigger checks, stops it, and only then runs the no-source external check.
Each acquisition runs for two seconds, with no post-start warmup. The
firmware-959 fixed two-second UDP-destination delay has been removed on this
branch, so the test exercises immediate readout startup.

```bash
./apps/examples/firmware_960_validation/scripts/build.sh
./apps/examples/firmware_960_validation/scripts/run.sh --firmware-only
./apps/examples/firmware_960_validation/scripts/run.sh
```

Results are written to `results/firmware_960_validation.json`. Override the
path with `--output PATH`.
