#!/bin/bash

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --debug) DEBUG=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Get the absolute path of the script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Path to the executable
EXECUTABLE="$SCRIPT_DIR/../build/bin/main"

# Check if the executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "Executable not found! Please run ./scripts/build.sh first."
    exit 1
fi

# Run with or without debugger based on the --debug flag
if [ "$DEBUG" == "true" ]; then
    echo "Running with debugger (gdb)..."
    gdb --args "$EXECUTABLE"
else
    echo "Running the executable..."
    "$EXECUTABLE"
fi
