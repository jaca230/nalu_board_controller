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
    // Default values as per the Python code
    std::string model = "HDSOCv1_evalr2";
    std::string board_ip_port = "192.168.1.59:4660";
    std::string host_ip_port = "192.168.1.1:4660";
    bool debug = false;

    // Set up logger
    NaluBoardControllerLogger::set_level(NaluBoardControllerLogger::LogLevel::DEBUG);

    // Register the signal handler for SIGINT
    std::signal(SIGINT, signal_handler);

    try {
        NaluBoardControllerLogger::info("Starting NaluBoardManager initialization...");

        // Step 1: Initialize NaluBoardManager
        NaluBoardControllerLogger::info("Creating board_manager object...");
        NaluBoardController board_manager(model, board_ip_port, host_ip_port, "", "", debug);

        NaluBoardControllerLogger::info("board_manager object created successfully.");

        // Step 2: Initialize the board
        NaluBoardControllerLogger::info("Calling initialize_board()...");
        board_manager.initialize_board();

        // Step 3: Initialize capture parameters using the new methods
        NaluBoardControllerLogger::info("Initializing capture parameters...");

        NaluCaptureParams capture_params;
        capture_params.target_ip_port = "192.168.1.1:12345";
        capture_params.channels = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
        capture_params.windows = 4;
        capture_params.lookback = 4;
        capture_params.write_after_trig = 4;
        capture_params.trigger_mode = "ext";
        capture_params.lookback_mode = "";
        capture_params.trigger_values = {};
        capture_params.dac_values = {};

        // Call the new start_capture method using capture_params
        NaluBoardControllerLogger::info("Starting board capture...");
        board_manager.start_capture(capture_params);

        // Wait for the signal to stop
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
