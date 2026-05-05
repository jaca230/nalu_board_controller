#include "nalu_board_controller/logging/logging.h"

#include <mutex>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace nalu_board_controller::logging {
namespace {

std::mutex logger_mutex;
const char* kDefaultPattern = "%Y-%m-%d %H:%M:%S.%e %n [%^%l%$] %v";

std::vector<spdlog::sink_ptr> build_sinks(bool console_enabled, const std::string& file_path) {
    std::vector<spdlog::sink_ptr> sinks;
    if (console_enabled) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (!file_path.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, true));
    }
    if (sinks.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
    }
    return sinks;
}

}  // namespace

void initialize() {
    std::lock_guard<std::mutex> lock(logger_mutex);
    auto logger = spdlog::default_logger();
    if (!logger) {
        auto sinks = build_sinks(true, "");
        logger = std::make_shared<spdlog::logger>("nalu_board_controller", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);
    }
    logger->flush_on(spdlog::level::warn);
}

void configure(const std::string& level,
               bool console_enabled,
               const std::string& file_path,
               const std::string& logger_name,
               const std::string& pattern) {
    std::lock_guard<std::mutex> lock(logger_mutex);
    auto sinks = build_sinks(console_enabled, file_path);
    auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
    logger->set_pattern(pattern.empty() ? kDefaultPattern : pattern);
    logger->set_level(spdlog::level::from_str(level));
    logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(logger);
}

spdlog::level::level_enum level_from_python(int level) {
    if (level >= 40) {
        return spdlog::level::err;
    }
    if (level >= 30) {
        return spdlog::level::warn;
    }
    if (level >= 20) {
        return spdlog::level::info;
    }
    return spdlog::level::debug;
}

}  // namespace nalu_board_controller::logging
