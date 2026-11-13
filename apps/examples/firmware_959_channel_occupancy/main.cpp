#include <atomic>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "nalu_board_controller/config/window_level_control_config.h"
#include "nalu_board_controller/logging/logging.h"

#include "../firmware_959_scan_support/scan_support.h"

namespace {

using firmware_959_scan::json;
std::atomic<bool> interrupted{false};

void handle_signal(int) {
    interrupted = true;
}

std::string resolve_path(const std::string& value, const std::filesystem::path& config_directory) {
    if (value.empty()) {
        return {};
    }
    const std::filesystem::path path(value);
    return path.is_absolute() ? path.lexically_normal().string()
                              : (config_directory / path).lexically_normal().string();
}

std::string attempt_raw_path(const std::string& raw_path, int attempt) {
    if (raw_path.empty()) {
        return {};
    }
    const std::filesystem::path path(raw_path);
    const std::string filename = path.stem().string() + ".attempt_" +
                                 std::to_string(attempt) + path.extension().string();
    return (path.parent_path() / filename).string();
}

void add_occupancy_summary(json& result, const std::vector<int>& channels) {
    std::vector<int> triggering_channels;
    json packet_counts = json::object();
    json trigger_counts = json::object();
    json trigger_rates = json::object();
    const json observed_packets = result.value("channel_packets", json::object());
    const json observed_triggers = result.value("channel_trigger_groups", json::object());
    const double elapsed = result.value("capture_elapsed_seconds", 0.0);
    for (int channel : channels) {
        const std::string key = std::to_string(channel);
        const uint64_t packets = observed_packets.value(key, uint64_t{0});
        const uint64_t triggers = observed_triggers.value(key, uint64_t{0});
        packet_counts[key] = packets;
        trigger_counts[key] = triggers;
        trigger_rates[key] = elapsed > 0 ? triggers / elapsed : 0.0;
        if (triggers > 0) {
            triggering_channels.push_back(channel);
        }
    }
    result["channel_packets"] = packet_counts;
    result["channel_trigger_groups"] = trigger_counts;
    result["channel_trigger_groups_per_second"] = trigger_rates;
    result["triggering_channels"] = triggering_channels;
    result["nontriggering_channels"] = json::array();
    for (int channel : channels) {
        if (std::find(triggering_channels.begin(), triggering_channels.end(), channel) ==
            triggering_channels.end()) {
            result["nontriggering_channels"].push_back(channel);
        }
    }
    result["configured_channel_count"] = channels.size();
    result["triggering_channel_count"] = triggering_channels.size();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        std::string config_path = "config.json";
        std::string output_override;
        double duration_override = 0.0;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (argument == "--output" && i + 1 < argc) {
                output_override = argv[++i];
            } else if (argument == "--duration" && i + 1 < argc) {
                duration_override = std::stod(argv[++i]);
            } else if (argument == "--help") {
                std::cout << "Usage: firmware_959_channel_occupancy [--config PATH] "
                             "[--duration SECONDS] [--output PATH]\n";
                return 0;
            } else {
                throw std::runtime_error("Unknown or incomplete argument: " + argument);
            }
        }

        const json document = firmware_959_scan::read_json(config_path);
        nalu_board_controller::logging::configure(document.value("log_level", "info"));
        const auto config_directory =
            std::filesystem::absolute(std::filesystem::path(config_path)).parent_path();
        const int channel_count = document.at("capture").value("channel_count", 32);
        std::vector<int> channels;
        channels.reserve(static_cast<size_t>(channel_count));
        for (int channel = 0; channel < channel_count; ++channel) {
            channels.push_back(channel);
        }

        const auto& run = document.at("run");
        const double duration =
            duration_override > 0 ? duration_override : run.value("duration_seconds", 10.0);
        const int windows = run.value("windows", 8);
        const bool wlc = run.value("wlc_enabled", false);
        const std::string raw_path = resolve_path(run.value("raw_output", ""), config_directory);
        const int max_retries = run.value("zero_event_max_retries", 5);
        const double retry_delay_seconds =
            run.value("zero_event_retry_delay_seconds", 2.0);
        const double restart_warmup_seconds =
            run.value("capture_restart_warmup_seconds", 2.0);

        nalu_board_controller::Controller controller(
            firmware_959_scan::board_config(document.at("board")));
        nalu_board_controller::WindowLevelControlConfig mode;
        mode.enabled = wlc;
        controller.set_window_level_control(mode);
        controller.initialize_board();

        json result;
        json attempts = json::array();
        int retry_count = 0;
        while (!interrupted) {
            const std::string current_raw_path = attempt_raw_path(raw_path, retry_count);
            result = firmware_959_scan::measure(
                controller,
                document,
                wlc,
                windows,
                channels,
                duration,
                firmware_959_scan::CaptureAction::ConfigureAndStart,
                true,
                restart_warmup_seconds,
                current_raw_path,
                interrupted);
            add_occupancy_summary(result, channels);

            const bool has_data = result.value("packets_total", uint64_t{0}) > 0 &&
                                  result.value("triggering_channel_count", size_t{0}) > 0;
            attempts.push_back({{"attempt", retry_count},
                                {"status", has_data ? "valid" : "zero_events"},
                                {"udp_datagrams", result.value("udp_datagrams", uint64_t{0})},
                                {"packets_total", result.value("packets_total", uint64_t{0})},
                                {"triggering_channel_count",
                                 result.value("triggering_channel_count", size_t{0})},
                                {"triggering_channels",
                                 result.value("triggering_channels", json::array())},
                                {"raw_output_path", current_raw_path}});
            if (has_data) {
                result["measurement_status"] = "valid";
                break;
            }
            if (retry_count >= max_retries) {
                result["measurement_status"] = "failed_after_retries";
                break;
            }

            ++retry_count;
            std::cerr << "No occupancy data received; retry " << retry_count << '/'
                      << max_retries << " after " << retry_delay_seconds << "s\n";
            const auto retry_deadline = firmware_959_scan::Clock::now() +
                                        std::chrono::duration<double>(retry_delay_seconds);
            while (!interrupted && firmware_959_scan::Clock::now() < retry_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        result["retry_count"] = retry_count;
        result["max_retries"] = max_retries;
        result["attempts"] = attempts;

        std::cout << result.dump(2) << '\n';

        const std::string configured_output =
            output_override.empty() ? document.value("output", "occupancy_data/channel_occupancy.json")
                                    : output_override;
        const std::string output_path = resolve_path(configured_output, config_directory);
        if (!output_path.empty()) {
            const std::filesystem::path output(output_path);
            if (output.has_parent_path()) {
                std::filesystem::create_directories(output.parent_path());
            }
            std::ofstream stream(output);
            if (!stream) {
                throw std::runtime_error("Cannot open output: " + output_path);
            }
            stream << result.dump(2) << '\n';
            std::cout << "Occupancy result: " << output_path << '\n';
        }
        if (interrupted) {
            return 130;
        }
        return result.value("measurement_status", "") == "valid" ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "firmware_959_channel_occupancy failed: " << error.what() << '\n';
        return 1;
    }
}
