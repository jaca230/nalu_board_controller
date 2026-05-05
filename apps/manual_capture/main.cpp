#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "nalu_board_controller/config/board_config.h"
#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/capture_config_builder.h"
#include "nalu_board_controller/config/channel_config.h"
#include "nalu_board_controller/controller/controller.h"
#include "nalu_board_controller/logging/logging.h"

namespace {

using json = nlohmann::json;

std::atomic<bool> running(true);

void signal_handler(int signal) {
    (void)signal;
    spdlog::info("Interrupt received. Stopping capture...");
    running = false;
}

void print_help() {
    std::cout << "Usage: manual_capture [options]\n"
              << "Options:\n"
              << "  --config PATH   Path to config.json\n"
              << "  --help          Show this help message\n";
}

json load_json(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    json document;
    stream >> document;
    return document;
}

template <typename T>
void assign_if_present(const json& object, const char* key, T& value) {
    const auto it = object.find(key);
    if (it != object.end() && !it->is_null()) {
        value = it->get<T>();
    }
}

nalu_board_controller::ChannelConfig merge_channel_config(
    const json& object,
    const nalu_board_controller::ChannelConfig& defaults) {
    nalu_board_controller::ChannelConfig config = defaults;
    assign_if_present(object, "enabled", config.enabled);
    assign_if_present(object, "trigger_value", config.trigger_value);
    assign_if_present(object, "dac_value", config.dac_value);
    return config;
}

nalu_board_controller::BoardConfig parse_board_config(const json& object) {
    nalu_board_controller::BoardConfig config;
    assign_if_present(object, "model", config.model);
    assign_if_present(object, "board_endpoint", config.board_endpoint);
    assign_if_present(object, "host_endpoint", config.host_endpoint);
    assign_if_present(object, "asic_serial_mode", config.asic_serial_mode);
    assign_if_present(object, "config_file", config.config_file);
    assign_if_present(object, "clock_file", config.clock_file);
    return config;
}

std::string resolve_relative_path(
    const std::string& path,
    const std::filesystem::path& config_directory) {
    if (path.empty()) {
        return path;
    }

    const std::filesystem::path candidate(path);
    if (candidate.is_absolute()) {
        return candidate.lexically_normal().string();
    }

    return (config_directory / candidate).lexically_normal().string();
}

nalu_board_controller::CaptureConfig parse_capture_config(const json& object) {
    nalu_board_controller::CaptureConfig config;
    assign_if_present(object, "target_endpoint", config.target_endpoint);
    assign_if_present(object, "assign_dac_values", config.assign_dac_values);

    if (const auto readout_it = object.find("readout_window");
        readout_it != object.end() && readout_it->is_object()) {
        assign_if_present(*readout_it, "windows", config.readout_window.windows);
        assign_if_present(*readout_it, "lookback", config.readout_window.lookback);
        assign_if_present(
            *readout_it, "write_after_trigger", config.readout_window.write_after_trigger);
        assign_if_present(*readout_it, "lookback_mode", config.readout_window.lookback_mode);
    }

    if (const auto trigger_it = object.find("trigger");
        trigger_it != object.end() && trigger_it->is_object()) {
        assign_if_present(*trigger_it, "mode", config.trigger.mode);
        assign_if_present(*trigger_it, "low_reference", config.trigger.low_reference);
        assign_if_present(*trigger_it, "high_reference", config.trigger.high_reference);
        assign_if_present(*trigger_it, "rising_edge", config.trigger.rising_edge);
    }

    if (const auto wlc_it = object.find("window_level_control");
        wlc_it != object.end() && wlc_it->is_object()) {
        assign_if_present(*wlc_it, "configure", config.window_level_control.configure);
        assign_if_present(*wlc_it, "enabled", config.window_level_control.enabled);
        assign_if_present(
            *wlc_it,
            "reinitialize_after_change",
            config.window_level_control.reinitialize_after_change);
    }

    if (const auto channels_it = object.find("channels");
        channels_it != object.end() && channels_it->is_object()) {
        int channel_count = 32;
        assign_if_present(*channels_it, "count", channel_count);

        nalu_board_controller::ChannelConfig defaults;
        if (const auto defaults_it = channels_it->find("defaults");
            defaults_it != channels_it->end() && defaults_it->is_object()) {
            defaults = merge_channel_config(*defaults_it, defaults);
        }

        auto builder = nalu_board_controller::CaptureConfigBuilder(channel_count);
        config.channels = builder.build().channels;
        for (auto& [channel, channel_config] : config.channels) {
            (void)channel;
            channel_config = defaults;
        }

        if (const auto overrides_it = channels_it->find("overrides");
            overrides_it != channels_it->end() && overrides_it->is_object()) {
            for (auto it = overrides_it->begin(); it != overrides_it->end(); ++it) {
                const int channel = std::stoi(it.key());
                config.channels[channel] = merge_channel_config(it.value(), defaults);
            }
        }
    }

    return config;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string config_path = "config.json";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                print_help();
                return 0;
            }
            if (arg == "--config") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--config requires a value");
                }
                config_path = argv[++i];
                continue;
            }
            throw std::runtime_error("Unknown argument: " + arg);
        }

        const json document = load_json(config_path);
        const std::filesystem::path config_directory =
            std::filesystem::absolute(std::filesystem::path(config_path)).parent_path();
        const json logging_config = document.value("logging", json::object());
        const std::string log_level = logging_config.value("level", "info");
        const std::string python_level = logging_config.value("python_level", log_level);

        nalu_board_controller::logging::configure(log_level);
        std::signal(SIGINT, signal_handler);

        auto board_config = parse_board_config(document.at("board"));
        board_config.config_file = resolve_relative_path(board_config.config_file, config_directory);
        board_config.clock_file = resolve_relative_path(board_config.clock_file, config_directory);
        const auto capture_config = parse_capture_config(document.at("capture"));

        nalu_board_controller::Controller controller(board_config);
        controller.initialize_board();
        controller.sync_python_logging(python_level);

        spdlog::info("Starting board capture using config '{}'", config_path);
        controller.start_capture(capture_config);
        spdlog::info("Capture started, hit Control-C to end capture...");

        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        controller.stop_capture();
        spdlog::info("Capture stopped.");
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("manual_capture failed: {}", error.what());
        return 1;
    }
}
