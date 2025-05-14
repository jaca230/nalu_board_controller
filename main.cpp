#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "nalu_board_controller.h"
#include "nalu_board_controller_logger.h"

std::atomic<bool> running(true);

void signal_handler(int signal) {
    NaluBoardControllerLogger::info("Interrupt received. Stopping capture...");
    running = false;
}

int main() {
    // Set up logger
    NaluBoardControllerLogger::set_level(NaluBoardControllerLogger::LogLevel::DEBUG);

    // Register the signal handler for SIGINT
    std::signal(SIGINT, signal_handler);

    try {
        NaluBoardControllerLogger::info("Starting NaluBoardManager initialization...");

        // Step 1: Initialize NaluBoardManager
        NaluBoardControllerLogger::info("Creating board_manager object...");
        NaluBoardParams board_params;
        board_params.model = "HDSoCv1_evalr2";
        board_params.board_ip_port = "192.168.1.59:4660";
        board_params.host_ip_port = "192.168.1.1:4660";
        board_params.config_file = "";
        board_params.clock_file = "";

        NaluBoardController board_manager(board_params);
        NaluBoardControllerLogger::info("board_manager object created successfully.");

        // Step 2: Initialize the board
        NaluBoardControllerLogger::info("Calling initialize_board()...");
        board_manager.initialize_board();

        // Step 3: Initialize capture parameters using the new methods
        NaluBoardControllerLogger::info("Initializing capture parameters...");

        int num_channels = 32;  // Set the number of channels to 32
        NaluCaptureParams capture_params = NaluCaptureParamsWrapper(num_channels).get_capture_params();

        int windows = 1;
        capture_params.target_ip_port = "192.168.1.1:12345";
        capture_params.assign_dac_values = false;
        capture_params.windows = windows;
        capture_params.lookback = windows;
        capture_params.write_after_trig = windows;
        capture_params.trigger_mode = "self";
        capture_params.lookback_mode = "";
        capture_params.low_reference = 3;
        capture_params.high_reference = 11;

        // Initialize the channels with the specified trigger values and DAC values
        for (int i = 0; i < num_channels; ++i) {
            NaluChannelInfo channel_info;

            // Set default trigger value to 0
            channel_info.trigger_value = 0;

            // Set trigger value to 123 for channel 2
            if (i == 2) {
                channel_info.trigger_value = 123;
            }

            // Set DAC value to 0 (or any default value)
            channel_info.dac_value = 0;

            // Enable all channels
            channel_info.enabled = true;

            // Add the channel info to the map, using the channel number (i) as the key
            capture_params.channels[i] = channel_info;
        }

        // Call the new start_capture method using capture_params
        NaluBoardControllerLogger::info("Starting board capture...");
        board_manager.start_capture(capture_params);

        // Wait for the signal to stop
        NaluBoardControllerLogger::info("Capture started, hit Control-C to end capture...");
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Step 4: Stop capture when interrupted
        board_manager.stop_capture();
        NaluBoardControllerLogger::info("Capture stopped.");

    } catch (const std::exception& e) {
        NaluBoardControllerLogger::error("Exception: " + std::string(e.what()));
    }

    return 0;
}
