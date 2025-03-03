#!/bin/bash

# Define the screen session name
SESSION_NAME="nalu_capture"

# Send Ctrl+C to gracefully stop the process inside the screen session
screen -S "$SESSION_NAME" -X stuff $'\003'

# Allow some time for the process to exit
sleep 2

# Stop the screen session
screen -S "$SESSION_NAME" -X quit

echo "Screen session '$SESSION_NAME' stopped."
