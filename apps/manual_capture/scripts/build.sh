#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
APP_DIR=$(realpath "$SCRIPT_DIR/..")
PROJECT_DIR=$(realpath "$APP_DIR/../..")
OVERWRITE=false

print_help() {
    cat <<EOF
Usage: ./apps/manual_capture/scripts/build.sh [options]

Build the manual_capture app.

Options:
  -o, --overwrite   Clean the build directory before building
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite) OVERWRITE=true; shift ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

BUILD_ARGS=(--apps)
if [ "$OVERWRITE" = true ]; then
    BUILD_ARGS+=(--overwrite)
fi

exec "$PROJECT_DIR/scripts/build.sh" "${BUILD_ARGS[@]}"
