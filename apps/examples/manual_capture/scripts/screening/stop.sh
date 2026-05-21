#!/bin/bash

set -euo pipefail

SESSION_NAME="manual_capture"

print_help() {
    cat <<EOF
Usage: ./apps/examples/manual_capture/scripts/screening/stop.sh [options]

Stop the detached manual_capture screen session.

Options:
  --session NAME    Screen session name (default: manual_capture)
  -h, --help        Show this help message
EOF
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --session) SESSION_NAME="$2"; shift 2 ;;
        -h|--help) print_help; exit 0 ;;
        *) echo "Unknown option: $1" >&2; echo; print_help; exit 1 ;;
    esac
done

screen -S "$SESSION_NAME" -X stuff $'\003' >/dev/null 2>&1 || true
sleep 2
screen -S "$SESSION_NAME" -X quit >/dev/null 2>&1 || true

echo "Screen session '$SESSION_NAME' stopped."
