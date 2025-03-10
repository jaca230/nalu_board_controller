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
        //NaluBoardController board_manager(board_params.model, board_params.board_ip_port, 
        //    board_params.host_ip_port, board_params.config_file, board_params.clock_file);

        NaluBoardControllerLogger::info("board_manager object created successfully.");

        // Step 2: Initialize the board
        NaluBoardControllerLogger::info("Calling initialize_board()...");
        board_manager.initialize_board();

        // Step 3: Initialize capture parameters using the new methods
        NaluBoardControllerLogger::info("Initializing capture parameters...");

        int num_channels = 16;
        NaluCaptureParams capture_params = NaluCaptureParamsWrapper(num_channels).get_capture_params();
        // capture_params.channels will be initialized as a map with num_channels keys each populated by default NaluChannelInfo struct

        int windows = 62;
        capture_params.target_ip_port = "192.168.1.1:12345";
        capture_params.assign_dac_values = false;
        capture_params.windows = windows;
        capture_params.lookback = windows;
        capture_params.write_after_trig = windows;
        capture_params.trigger_mode = "ext";
        capture_params.lookback_mode = "";

        // Example of how to manually set channels and their trigger/dac values
        // This is not necessary since we called NaluCaptureParamsWrapper(num_channels) which initializes the channels map
        // To the exact same thing we set it to below.
        for (int i = 0; i < num_channels; ++i) {
            NaluChannelInfo channel_info;

            // Customize trigger_value and dac_value per channel
            channel_info.trigger_value = 0;
            channel_info.dac_value = 0;

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
