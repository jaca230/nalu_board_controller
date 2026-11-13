# Firmware 960 acquisition validation

This short hardware test verifies the firmware readback and exercises the
acquisition paths needed before removing firmware-959 workarounds:

- normal self-trigger acquisition produces data;
- WLC self-trigger acquisition produces data;
- external-trigger acquisition with no source returns zero data and exits
  cleanly.

The self-trigger checks also validate event shape and multiplicity. Normal mode
expects the requested window count directly. WLC mode clamps the ASIC request
to 1–12 and expects `2 * ceil(requested / 2) + 2` returned packets per
channel-trigger. Thus the default request of 8 must return 10 packets per
channel-trigger, and the maximum request of 12 returns 14. Both modes must also
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
