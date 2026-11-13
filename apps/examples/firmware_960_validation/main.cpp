#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "nalu_board_controller/config/wlc_window_mapping.h"
#include "nalu_board_controller/config/window_level_control_config.h"
#include "nalu_board_controller/controller/controller.h"
#include "nalu_board_controller/logging/logging.h"

#include "../firmware_959_scan_support/scan_support.h"

namespace {

using firmware_959_scan::json;
using Clock = std::chrono::steady_clock;
std::atomic<bool> interrupted{false};

std::string resolve_path(const std::string& value,
                         const std::filesystem::path& config_directory) {
    if (value.empty()) {
        return {};
    }
    const std::filesystem::path path(value);
    return path.is_absolute() ? path.lexically_normal().string()
                              : (config_directory / path).lexically_normal().string();
}

void write_json(const std::string& path, const json& value) {
    if (path.empty()) {
        return;
    }
    const std::filesystem::path output(path);
    if (output.has_parent_path()) {
        std::filesystem::create_directories(output.parent_path());
    }
    std::ofstream stream(output);
    if (!stream) {
        throw std::runtime_error("Cannot write validation output: " + path);
    }
    stream << value.dump(2) << '\n';
}

json run_scenario(nalu_board_controller::Controller& controller,
                  const json& document,
                  const json& scenario,
                  const std::vector<int>& channels,
                  int configured_windows,
                  double duration_seconds) {
    json scenario_document = document;
    const std::string trigger_mode = scenario.at("trigger_mode").get<std::string>();
    const bool wlc_enabled = scenario.at("wlc_enabled").get<bool>();
    const bool expect_data = scenario.at("expect_data").get<bool>();
    const int asic_requested_windows = wlc_enabled
        ? nalu_board_controller::wlc_asic_windows_for_returned(configured_windows)
        : configured_windows;
    const int expected_returned_windows = configured_windows;
    scenario_document["capture"]["trigger_mode"] = trigger_mode;

    const auto started = Clock::now();
    json result = firmware_959_scan::measure(
        controller,
        scenario_document,
        wlc_enabled,
        configured_windows,
        channels,
        duration_seconds,
        firmware_959_scan::CaptureAction::ConfigureAndStart,
        true,
        0.0,
        "",
        interrupted);
    const double wall_seconds = std::chrono::duration<double>(Clock::now() - started).count();
    const uint64_t packets = result.value("packets_total", uint64_t{0});
    uint64_t channel_trigger_groups = 0;
    const json observed_channel_groups =
        result.value("channel_trigger_groups", json::object());
    for (auto iterator = observed_channel_groups.begin();
         iterator != observed_channel_groups.end();
         ++iterator) {
        channel_trigger_groups += iterator.value().get<uint64_t>();
    }
    const double observed_windows_per_channel_trigger = channel_trigger_groups > 0
        ? static_cast<double>(packets) / static_cast<double>(channel_trigger_groups)
        : 0.0;
    const double window_ratio = expected_returned_windows > 0
        ? observed_windows_per_channel_trigger / expected_returned_windows
        : 0.0;
    const double expected_trigger_rate = scenario.value("expected_trigger_rate_hz", 0.0);
    const double observed_trigger_rate = result.value("trigger_groups_per_second", 0.0);
    const double trigger_rate_ratio = expected_trigger_rate > 0.0
        ? observed_trigger_rate / expected_trigger_rate
        : 0.0;
    const auto& run = document.at("run");
    const double window_tolerance = run.value("window_ratio_tolerance", 0.02);
    const double trigger_rate_tolerance =
        run.value("trigger_rate_ratio_tolerance", 0.05);
    const bool data_expectation_passed = expect_data ? packets > 0 : packets == 0;
    const bool window_count_passed = !expect_data ||
        std::abs(window_ratio - 1.0) <= window_tolerance;
    const bool trigger_rate_passed = expected_trigger_rate <= 0.0 ||
        std::abs(trigger_rate_ratio - 1.0) <= trigger_rate_tolerance;
    const bool passed = data_expectation_passed && window_count_passed && trigger_rate_passed;
    result["name"] = scenario.at("name");
    result["trigger_mode"] = trigger_mode;
    result["wlc_enabled"] = wlc_enabled;
    result["expect_data"] = expect_data;
    result["configured_windows"] = configured_windows;
    result["requested_windows"] = configured_windows;
    result["asic_requested_windows"] = asic_requested_windows;
    result["expected_returned_windows"] = expected_returned_windows;
    result["observed_windows_per_channel_trigger"] =
        observed_windows_per_channel_trigger;
    result["window_count_ratio"] = window_ratio;
    result["window_count_status"] = window_count_passed ? "passed" : "failed";
    if (expected_trigger_rate > 0.0) {
        result["expected_trigger_rate_hz"] = expected_trigger_rate;
        result["observed_trigger_rate_hz"] = observed_trigger_rate;
        result["trigger_rate_ratio"] = trigger_rate_ratio;
        result["trigger_rate_status"] = trigger_rate_passed ? "passed" : "failed";
    }
    result["wall_seconds"] = wall_seconds;
    result["status"] = passed ? "passed" : "failed";
    if (!passed) {
        result["failures"] = json::array();
        if (!data_expectation_passed) {
            result["failures"].push_back(
                expect_data ? "expected packets but received none"
                            : "expected zero packets without an external trigger");
        }
        if (!window_count_passed) {
            result["failures"].push_back("packet multiplicity does not match expected windows");
        }
        if (!trigger_rate_passed) {
            result["failures"].push_back(
                "observed trigger rate does not match the Pico rate (possible duplicate triggers)");
        }
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string config_path = "config.json";
        std::string output_override;
        std::vector<std::string> selected_scenarios;
        bool firmware_only = false;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--config" && index + 1 < argc) {
                config_path = argv[++index];
            } else if (argument == "--output" && index + 1 < argc) {
                output_override = argv[++index];
            } else if (argument == "--firmware-only") {
                firmware_only = true;
            } else if (argument == "--scenario" && index + 1 < argc) {
                selected_scenarios.push_back(argv[++index]);
            } else if (argument == "--help") {
                std::cout << "Usage: firmware_960_validation [--config PATH] [--output PATH] "
                             "[--firmware-only] [--scenario NAME ...]\n";
                return 0;
            } else {
                throw std::runtime_error("Unknown or incomplete argument: " + argument);
            }
        }

        const json document = firmware_959_scan::read_json(config_path);
        nalu_board_controller::logging::configure(document.value("log_level", "info"));
        const auto config_directory =
            std::filesystem::absolute(std::filesystem::path(config_path)).parent_path();
        const auto& run = document.at("run");
        const std::string output_path = output_override.empty()
            ? resolve_path(run.value("output", ""), config_directory)
            : output_override;

        nalu_board_controller::Controller controller(
            firmware_959_scan::board_config(document.at("board")));
        nalu_board_controller::WindowLevelControlConfig initial_mode;
        initial_mode.enabled = false;
        controller.set_window_level_control(initial_mode);
        controller.initialize_board();

        const std::string firmware = controller.read_firmware_version();
        const std::string expected = document.at("expected_firmware_substring");
        const bool firmware_matches = firmware.find(expected) != std::string::npos;
        json report = {{"expected_firmware_substring", expected},
                       {"firmware_readback", firmware},
                       {"firmware_status", firmware_matches ? "passed" : "failed"},
                       {"scenarios", json::array()}};
        if (!firmware_matches) {
            report["status"] = "failed";
            report["failure"] = "firmware readback did not contain expected value";
            write_json(output_path, report);
            std::cerr << report.dump(2) << '\n';
            return 1;
        }
        if (firmware_only) {
            report["status"] = "passed";
            write_json(output_path, report);
            std::cout << report.dump(2) << '\n';
            return 0;
        }

        const auto channels = run.at("channels").get<std::vector<int>>();
        const int requested_windows = run.value("requested_windows", 8);
        const double duration_seconds = run.value("duration_seconds", 2.0);
        bool all_passed = true;
        for (const auto& scenario : run.at("scenarios")) {
            const std::string name = scenario.at("name");
            if (!selected_scenarios.empty() &&
                std::find(selected_scenarios.begin(), selected_scenarios.end(), name) ==
                    selected_scenarios.end()) {
                continue;
            }
            json result = run_scenario(
                controller, document, scenario, channels, requested_windows, duration_seconds);
            all_passed = all_passed && result.at("status") == "passed";
            report["scenarios"].push_back(std::move(result));
        }
        if (!selected_scenarios.empty() && report["scenarios"].size() != selected_scenarios.size()) {
            throw std::runtime_error("One or more requested scenario names were not found");
        }
        report["status"] = all_passed ? "passed" : "failed";
        write_json(output_path, report);
        std::cout << report.dump(2) << '\n';
        return all_passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "firmware_960_validation failed: " << error.what() << '\n';
        return 1;
    }
}
