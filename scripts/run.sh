#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

print_help() {
    cat <<EOF
Usage: ./scripts/run.sh <app> [app options]

Run one of the app-specific launchers.

Apps:
  manual_capture   Launch apps/manual_capture/scripts/run.sh
  packet_listener  Launch apps/packet_listener/scripts/run.sh

Examples:
  ./apps/manual_capture/scripts/build.sh
  ./scripts/run.sh manual_capture
  ./apps/packet_listener/scripts/build.sh
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
    manual_capture|packet_listener)
        APP_NAME="$1"
        shift
        APP_RUNNER="$PROJECT_DIR/apps/$APP_NAME/scripts/run.sh"
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
