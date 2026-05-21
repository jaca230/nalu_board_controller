#!/bin/bash

SCRIPT_PATH=$(realpath "${BASH_SOURCE[0]}")
SCRIPT_DIR=$(dirname "$SCRIPT_PATH")
APP_DIR=$(realpath "$SCRIPT_DIR/../..")
VENV_PATH="$APP_DIR/.venv"

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "This script must be sourced:" >&2
    echo "  source ./apps/examples/manual_capture/scripts/environment/activate_venv.sh" >&2
    exit 1
fi

if [ ! -f "$VENV_PATH/bin/activate" ]; then
    echo "Virtual environment not found: $VENV_PATH" >&2
    echo "Create it with ./apps/examples/manual_capture/scripts/environment/setup_venv.sh" >&2
    return 1
fi

# Keep app runtime isolated from user-site Python packages.
export PYTHONNOUSERSITE=1
unset PYTHONHOME

# shellcheck disable=SC1090
source "$VENV_PATH/bin/activate"
