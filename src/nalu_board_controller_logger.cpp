#include "nalu_board_controller_logger.h"

NaluBoardControllerLogger::LogLevel NaluBoardControllerLogger::log_level = NaluBoardControllerLogger::LogLevel::INFO;
std::ofstream NaluBoardControllerLogger::log_file;

// Default colors
std::string NaluBoardControllerLogger::debug_color = "\033[0;37m";  // White
std::string NaluBoardControllerLogger::info_color = "\033[0;37m";   // White
std::string NaluBoardControllerLogger::warning_color = "\033[1;33m"; // Yellow
std::string NaluBoardControllerLogger::error_color = "\033[1;31m";   // Red


// Set log level
void NaluBoardControllerLogger::set_level(LogLevel level) {
    log_level = level;
}

// Enable file logging
void NaluBoardControllerLogger::enable_file_logging(const std::string& filename) {
    log_file.open(filename, std::ios::out | std::ios::app);
    if (!log_file) {
        std::cerr << "[ERROR] Failed to open log file: " << filename << std::endl;
    }
}

// Disable file logging
void NaluBoardControllerLogger::disable_file_logging() {
    if (log_file.is_open()) log_file.close();
}

// Set log colors
void NaluBoardControllerLogger::set_log_colors(const std::string& debug_color, const std::string& info_color, 
                                               const std::string& warning_color, const std::string& error_color) {
    NaluBoardControllerLogger::debug_color = debug_color;
    NaluBoardControllerLogger::info_color = info_color;
    NaluBoardControllerLogger::warning_color = warning_color;
    NaluBoardControllerLogger::error_color = error_color;
}

// Debug message
void NaluBoardControllerLogger::debug(const std::string& message) {
    log("[DEBUG] ", message, LogLevel::DEBUG);
}

// Info message
void NaluBoardControllerLogger::info(const std::string& message) {
    log("[INFO]  ", message, LogLevel::INFO);
}

// Warning message
void NaluBoardControllerLogger::warning(const std::string& message) {
    log("[WARN]  ", message, LogLevel::WARNING);
}

// Error message
void NaluBoardControllerLogger::error(const std::string& message) {
    log("[ERROR] ", message, LogLevel::ERROR);
}

// General log method
void NaluBoardControllerLogger::log(const std::string& prefix, const std::string& message, LogLevel level) {
    if (level >= log_level) {
        std::string log_message = prefix + message;
        std::string colored_message = get_colored_message(log_message, get_color_for_level(level));
        std::cout << colored_message << std::endl;
        if (log_file.is_open()) {
            log_file << log_message << std::endl;
        }
    }
}

// Get the colored message based on log level
std::string NaluBoardControllerLogger::get_colored_message(const std::string& message, const std::string& color) {
    return color + message + "\033[0m";  // Reset color to default
}

// Get color based on log level
std::string NaluBoardControllerLogger::get_color_for_level(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return debug_color;
        case LogLevel::INFO:
            return info_color;
        case LogLevel::WARNING:
            return warning_color;
        case LogLevel::ERROR:
            return error_color;
        default:
            return "\033[0m";  // No color (reset)
    }
}
