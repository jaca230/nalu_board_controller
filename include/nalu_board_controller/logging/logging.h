#ifndef NALU_BOARD_CONTROLLER_LOGGING_LOGGING_H
#define NALU_BOARD_CONTROLLER_LOGGING_LOGGING_H

#include <string>

#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

namespace nalu_board_controller::logging {

void initialize();
void configure(
    const std::string& level = "info",
    bool console_enabled = true,
    const std::string& file_path = "",
    const std::string& logger_name = "nalu_board_controller",
    const std::string& pattern = "%Y-%m-%d %H:%M:%S.%e %n [%^%l%$] %v");
void add_sink(const spdlog::sink_ptr& sink);
spdlog::level::level_enum level_from_python(int level);

}  // namespace nalu_board_controller::logging

#endif
