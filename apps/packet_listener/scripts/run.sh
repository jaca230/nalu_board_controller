#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
PROJECT_DIR=$(realpath "$APP_DIR/../..")
EXECUTABLE="$PROJECT_DIR/build/bin/packet_listener"
CONFIG_PATH="$APP_DIR/config.json"
DEBUG=false

print_help() {
    cat <<EOF
Usage: ./apps/packet_listener/scripts/run.sh [options]

Run the packet listener app.

Options:
  --config PATH     Path to config.json (default: apps/packet_listener/config.json)
  --debug           Run under gdb
  -h, --help        Show this help message

Build first with:
  ./apps/packet_listener/scripts/build.sh
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --config) CONFIG_PATH="$2"; shift 2 ;;
        --debug) DEBUG=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found: $EXECUTABLE" >&2
    echo "Build it with ./apps/packet_listener/scripts/build.sh." >&2
    exit 1
fi

if [ ! -f "$CONFIG_PATH" ]; then
    echo "Config file not found: $CONFIG_PATH" >&2
    exit 1
fi

if [ "$DEBUG" = true ]; then
    exec gdb --args "$EXECUTABLE" --config "$CONFIG_PATH"
else
    exec "$EXECUTABLE" --config "$CONFIG_PATH"
fi
