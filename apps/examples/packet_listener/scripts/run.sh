#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
EXECUTABLE="$APP_DIR/build/bin/packet_listener"
CONFIG_PATH="$APP_DIR/config.json"
DEBUG=false
SAMPLE_COUNT=""
SUMMARY_ONLY=false
CHANNELS=""

print_help() {
    cat <<EOF
Usage: ./apps/examples/packet_listener/scripts/run.sh [options]

Run the packet listener app.

Options:
  --config PATH       Path to config.json
                      (default: apps/examples/packet_listener/config.json)

  --sample-count N    Stop after parsing N packets and print a summary

  --summary-only      Suppress per-packet logs and print only the summary

  --channels LIST     Comma-separated channel list to display
                      Example: --channels 0,1,5,12

  --debug             Run under gdb

  -h, --help          Show this help message

Build first with:
  ./apps/examples/packet_listener/scripts/build.sh
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --config)
            CONFIG_PATH="$2"
            shift 2
            ;;

        --sample-count)
            SAMPLE_COUNT="$2"
            shift 2
            ;;

        --summary-only)
            SUMMARY_ONLY=true
            shift
            ;;

        --channels)
            CHANNELS="$2"
            shift 2
            ;;

        --debug)
            DEBUG=true
            shift
            ;;

        -h|--help)
            print_help
            exit 0
            ;;

        *)
            echo "Unknown option: $1" >&2
            echo
            print_help
            exit 1
            ;;
    esac
done

if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found: $EXECUTABLE" >&2
    echo "Build it with ./apps/examples/packet_listener/scripts/build.sh." >&2
    exit 1
fi

if [ ! -f "$CONFIG_PATH" ]; then
    echo "Config file not found: $CONFIG_PATH" >&2
    exit 1
fi

RUN_ARGS=(--config "$CONFIG_PATH")

if [ -n "$SAMPLE_COUNT" ]; then
    RUN_ARGS+=(--sample-count "$SAMPLE_COUNT")
fi

if [ "$SUMMARY_ONLY" = true ]; then
    RUN_ARGS+=(--summary-only)
fi

if [ -n "$CHANNELS" ]; then
    RUN_ARGS+=(--channels "$CHANNELS")
fi

if [ "$DEBUG" = true ]; then
    exec gdb --args "$EXECUTABLE" "${RUN_ARGS[@]}"
else
    exec "$EXECUTABLE" "${RUN_ARGS[@]}"
fi