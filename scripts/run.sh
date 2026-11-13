#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

print_help() {
    cat <<EOF
Usage: ./scripts/run.sh <app> [app options]

Run one of the app-specific launchers.

Apps:
  manual_capture   Launch apps/examples/manual_capture/scripts/run.sh
  packet_listener  Launch apps/examples/packet_listener/scripts/run.sh
  firmware_959_deadtime_scan  Launch the standalone normal/WLC deadtime scan
  firmware_959_rate_scan  Launch the single-pulse rate/efficiency scan
  firmware_959_channel_occupancy  Check nominal all-channel self-trigger occupancy
  firmware_960_validation  Validate firmware readback and short acquisition modes

Examples:
  ./apps/examples/manual_capture/scripts/build.sh
  ./scripts/run.sh manual_capture
  ./apps/examples/packet_listener/scripts/build.sh
  ./scripts/run.sh packet_listener
EOF
}

if [ "$#" -eq 0 ]; then
    print_help
    exit 1
fi

case "$1" in
    -h|--help)
        print_help
        exit 0
        ;;
    manual_capture|packet_listener|firmware_959_deadtime_scan|firmware_959_rate_scan|firmware_959_channel_occupancy|firmware_960_validation)
        APP_NAME="$1"
        shift
        APP_RUNNER="$PROJECT_DIR/apps/examples/$APP_NAME/scripts/run.sh"
        if [ ! -x "$APP_RUNNER" ]; then
            echo "App runner not found or not executable: $APP_RUNNER" >&2
            exit 1
        fi
        exec "$APP_RUNNER" "$@"
        ;;
    *)
        echo "Unknown app: $1" >&2
        echo >&2
        print_help
        exit 1
        ;;
esac
