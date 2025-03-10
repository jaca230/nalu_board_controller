#ifndef NALU_BOARD_CONTROLLER_LOGGER_H
#define NALU_BOARD_CONTROLLER_LOGGER_H

#include <iostream>
#include <fstream>
#include <string>

class NaluBoardControllerLogger {
public:
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    static void set_level(LogLevel level);
    static void set_level(const std::string& level);
    static void enable_file_logging(const std::string& filename);
    static void disable_file_logging();

    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

    static void set_log_colors(const std::string& debug_color, const std::string& info_color, 
                                const std::string& warning_color, const std::string& error_color);

private:
    static LogLevel log_level;
    static std::ofstream log_file;
    
    static std::string debug_color;
    static std::string info_color;
    static std::string warning_color;
    static std::string error_color;

    static void log(const std::string& prefix, const std::string& message, LogLevel level);
    static std::string get_colored_message(const std::string& message, const std::string& color);
    
    // This is the missing private method
    static std::string get_color_for_level(LogLevel level);
};

#endif  // NALU_BOARD_CONTROLLER_LOGGER_H
