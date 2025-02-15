#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <arpa/inet.h>  // For inet_pton()

#include "nalu_board_manager.h"
#include "nalu_board_controller_logger.h"
#include "ip_address_info.h"

void capture_packets(const std::string& target_ip_port) {
    try {
        IPAddressInfo target_ip_port_pair = IPAddressInfo(target_ip_port);
        std::string target_ip = target_ip_port_pair.getIp();  // Extract the IP
        int target_port = target_ip_port_pair.getPort();       // Extract the port
        NaluBoardControllerLogger::info("Starting packet capture...");

        // Setup UDP socket
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            NaluBoardControllerLogger::error("Failed to create socket.");
            return;
        }

        // Setup target address (bind address)
        struct sockaddr_in target_addr;
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(target_port);

        // Convert target_ip to binary form and set it to target_addr.sin_addr.s_addr
        if (inet_pton(AF_INET, target_ip.c_str(), &target_addr.sin_addr) <= 0) {
            NaluBoardControllerLogger::error("Invalid IP address: " + target_ip);
            close(sockfd);
            return;
        }

        // Bind the socket to the address and port
        if (bind(sockfd, (struct sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
            NaluBoardControllerLogger::error("Failed to bind socket to port.");
            close(sockfd);
            return;
        }

        // Log the IP and port we're listening on
        NaluBoardControllerLogger::info("Listening on IP: " + target_ip + ", Port: " + std::to_string(target_port));

        // Buffer to hold incoming data
        const int MAX_BYTES = 1040 * 3;
        const int TIMEOUT_SECONDS = 10;
        int received_bytes = 0;
        char buffer[2048];  // buffer to hold the packet data

        auto start_time = std::chrono::high_resolution_clock::now();

        NaluBoardControllerLogger::info("Listening for packets...");

        while (received_bytes < MAX_BYTES) {
            // Check if the timeout has passed
            auto elapsed_time = std::chrono::high_resolution_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed_time).count() >= TIMEOUT_SECONDS) {
                NaluBoardControllerLogger::info("Timeout reached: 10 seconds passed.");
                break;
            }

            // Receive a packet
            int len = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
            if (len < 0) {
                NaluBoardControllerLogger::error("Failed to receive packet.");
                break;
            }

            // Accumulate received bytes
            received_bytes += len;
            NaluBoardControllerLogger::info("Received " + std::to_string(received_bytes) + " bytes.");
        }

        // Print results after capture completes
        if (received_bytes >= MAX_BYTES) {
            NaluBoardControllerLogger::info("Captured " + std::to_string(received_bytes) + " bytes.");
        } else {
            NaluBoardControllerLogger::info("Capture stopped early after receiving " + std::to_string(received_bytes) + " bytes.");
        }

        // Close the socket
        close(sockfd);

    } catch (const std::exception& e) {
        NaluBoardControllerLogger::error("Exception in packet capture: " + std::string(e.what()));
    }
}


int main() {
    // Default values as per the Python code
    std::string model = "HDSOCv1_evalr2";
    std::string board_ip_port = "192.168.1.59:4660";
    std::string host_ip_port = "192.168.1.1:4660";
    bool debug = false;

    // Capture settings (you can modify these as needed)
    std::string target_ip_port = "192.168.1.1:12345";
    std::vector<int> channels = {0, 1, 2, 3}; // Example channel list
    int windows = 8;
    int lookback = 8;
    int write_after_trig = 8;
    std::string trigger_mode = "ext";
    std::string lookback_mode = "";
    std::vector<int> trigger_values = {};
    std::vector<int> dac_values = {};

    // Set up logger
    NaluBoardControllerLogger::set_level(NaluBoardControllerLogger::LogLevel::DEBUG);

    try {
        NaluBoardControllerLogger::info("Starting NaluBoardManager initialization...");

        // Step 1: Initialize NaluBoardManager
        NaluBoardControllerLogger::info("Creating board_manager object...");
        NaluBoardManager board_manager(model, board_ip_port, host_ip_port, "", "", debug);
        
        NaluBoardControllerLogger::info("board_manager object created successfully.");

        // Step 2: Initialize the board
        NaluBoardControllerLogger::info("Calling initialize_board()...");
        board_manager.initialize_board();
        
        // Step 3: Initialize capture parameters (no need to use start_capture with parameters)
        NaluBoardControllerLogger::info("Initializing capture parameters...");
        board_manager.init_capture(target_ip_port, channels, windows, lookback, write_after_trig, trigger_mode, 
                                   lookback_mode, trigger_values, dac_values);

        NaluBoardControllerLogger::info("Starting board capture...");
        board_manager.start_capture();
        
        // Step 4: Start capture by listening to the target address
        NaluBoardControllerLogger::info("Starting packet capture...");
        capture_packets(target_ip_port);  // Start listening for packets
        
        // Step 5: Stop capture (we handle stopping within the capture_packets function)
        board_manager.stop_capture();
        NaluBoardControllerLogger::info("Capture stopped.");

    } catch (const std::exception& e) {
        NaluBoardControllerLogger::error("Exception caught: " + std::string(e.what()));
    }

    NaluBoardControllerLogger::info("Program finished execution.");
    return 0;
}
