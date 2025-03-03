#!/bin/bash

# Define the screen session name
SESSION_NAME="nalu_capture"

# Stop any existing screen session with the same name
screen -S "$SESSION_NAME" -X quit

# Start a new detached screen session running the script
screen -dmS "$SESSION_NAME" ./run.sh

echo "Screen session '$SESSION_NAME' started."
