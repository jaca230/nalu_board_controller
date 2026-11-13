#include <atomic>
#include <csignal>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nalu_board_controller/config/window_level_control_config.h"
#include "nalu_board_controller/logging/logging.h"

#include "../firmware_959_scan_support/scan_support.h"
#include "event_analysis.h"

namespace {

using firmware_959_scan::json;
std::atomic<bool> interrupted{false};

void handle_signal(int) {
    interrupted = true;
}

std::vector<int> parse_channels(const std::string& value) {
    std::vector<int> channels;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        channels.push_back(std::stoi(item));
    }
    if (channels.empty()) {
        throw std::runtime_error("--channels requires a comma-separated channel list");
    }
    return channels;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        std::string config_path;
        std::string analyze_raw_path;
        std::vector<int> analyze_channels;
        int analyze_windows = 0;
        uint64_t analyze_start_ns = 0;
        uint64_t analyze_end_ns = std::numeric_limits<uint64_t>::max();
        bool serve = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (argument == "--serve") {
                serve = true;
            } else if (argument == "--analyze-raw" && i + 1 < argc) {
                analyze_raw_path = argv[++i];
            } else if (argument == "--windows" && i + 1 < argc) {
                analyze_windows = std::stoi(argv[++i]);
            } else if (argument == "--channels" && i + 1 < argc) {
                analyze_channels = parse_channels(argv[++i]);
            } else if (argument == "--measurement-start-ns" && i + 1 < argc) {
                analyze_start_ns = std::stoull(argv[++i]);
            } else if (argument == "--measurement-end-ns" && i + 1 < argc) {
                analyze_end_ns = std::stoull(argv[++i]);
            } else {
                throw std::runtime_error("Unknown or incomplete argument: " + argument);
            }
        }
        if (config_path.empty()) {
            throw std::runtime_error("Required: --config PATH");
        }
        const json document = firmware_959_scan::read_json(config_path);
        if (!analyze_raw_path.empty()) {
            if (analyze_windows <= 0 || analyze_channels.empty()) {
                throw std::runtime_error(
                    "--analyze-raw also requires --windows and --channels");
            }
            std::cout << firmware_959_rate_scan::analyze_raw_events(
                             analyze_raw_path,
                             document.at("receiver").value("udp_header_bytes", size_t{16}),
                             analyze_start_ns,
                             analyze_end_ns,
                             analyze_windows,
                             analyze_channels,
                             document.value("event_analysis", json::object()))
                             .dump(2)
                      << '\n';
            return 0;
        }
        if (!serve) {
            throw std::runtime_error("Use --serve; the Python scan driver manages requests");
        }

        nalu_board_controller::logging::configure(document.value("log_level", "info"));
        nalu_board_controller::Controller controller(
            firmware_959_scan::board_config(document.at("board")));

        bool initialized = false;
        bool capture_running = false;
        json previous_hardware;
        std::string line;
        while (!interrupted && std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }
            const json request = json::parse(line);
            const bool wlc = request.at("wlc_enabled").get<bool>();
            const int windows = request.at("requested_windows").get<int>();
            const auto channels = request.at("active_channels").get<std::vector<int>>();
            const double duration = request.at("duration_seconds").get<double>();
            const std::string raw_output_path = request.value("raw_output_path", "");
            const bool force_reconfigure = request.value("force_reconfigure", false);
            const double capture_restart_warmup_seconds =
                request.value("capture_restart_warmup_seconds", 0.0);
            const json hardware_key = {{"wlc_enabled", wlc},
                                       {"requested_windows", windows},
                                       {"active_channels", channels}};

            if (!initialized) {
                nalu_board_controller::WindowLevelControlConfig mode;
                mode.enabled = wlc;
                controller.set_window_level_control(mode);
                controller.initialize_board();
                initialized = true;
            }

            const bool changed = previous_hardware.is_null() || previous_hardware != hardware_key;
            const bool restart_capture = !capture_running || changed || force_reconfigure;
            if (capture_running && restart_capture) {
                controller.stop_capture();
                capture_running = false;
            }
            json result = firmware_959_scan::measure(controller,
                                                      document,
                                                      wlc,
                                                      windows,
                                                      channels,
                                                      duration,
                                                      restart_capture
                                                          ? firmware_959_scan::CaptureAction::ConfigureAndStart
                                                          : firmware_959_scan::CaptureAction::AlreadyRunning,
                                                      false,
                                                      capture_restart_warmup_seconds,
                                                      raw_output_path,
                                                      interrupted);
            capture_running = true;
            result["hardware_reconfigured"] = restart_capture;
            result["forced_reconfiguration"] = force_reconfigure;
            result["capture_continued"] = !restart_capture;
            if (!raw_output_path.empty()) {
                result["event_analysis"] = firmware_959_rate_scan::analyze_raw_events(
                    raw_output_path,
                    document.at("receiver").value("udp_header_bytes", size_t{16}),
                    result.at("measurement_start_offset_ns").get<uint64_t>(),
                    result.at("measurement_end_offset_ns").get<uint64_t>(),
                    windows,
                    channels,
                    document.value("event_analysis", json::object()));
            }
            previous_hardware = hardware_key;
            std::cout << "RESULT_JSON:" << result.dump() << std::endl;
        }
        if (capture_running) {
            controller.stop_capture();
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "firmware_959_rate_point failed: " << error.what() << '\n';
        return 1;
    }
}
