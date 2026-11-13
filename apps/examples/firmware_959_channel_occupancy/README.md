# Firmware 959 channel occupancy check

This app enables all 32 channels with the nominal firmware-959 self-trigger
settings used by the current DAQ setup: 8 windows, trigger value 1, DAC 1804,
references 5–11, rising edge, and WLC disabled. It captures for 10 seconds and
reports packet and unique trigger-group counts for every channel, including
explicit zeros.

If an attempt receives no packets, the app stops and fully reconfigures capture,
waits two seconds, and retries up to five times. Attempt status and counts are
stored in the final result under `attempts` and `retry_count`.

No Pico pulser is used. The purpose is to identify which channels self-trigger
under the nominal all-enabled configuration.

```bash
./apps/examples/firmware_959_channel_occupancy/scripts/build.sh
./apps/examples/firmware_959_channel_occupancy/scripts/run.sh
```

Override the duration or result path when needed:

```bash
./apps/examples/firmware_959_channel_occupancy/scripts/run.sh \
  --duration 30 --output /tmp/channel_occupancy.json
```

Results default to `occupancy_data/channel_occupancy.json`. Set
`run.raw_output` in `config.json` if a framed `NALUUDP1` raw capture is also
desired.
