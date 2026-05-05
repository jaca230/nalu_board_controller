#include "nalu_board_controller/runtime/state.h"

#include <algorithm>
#include <cctype>

namespace {

std::string to_lower_copy(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

}  // namespace

namespace nalu_board_controller {

State::State(const BoardConfig& config)
    : model_(to_lower_copy(config.model)),
      board_address_(config.board_endpoint),
      host_address_(config.host_endpoint),
      config_file_(config.config_file),
      clock_file_(config.clock_file) {}

void State::update_from_capture_config(const CaptureConfig& config) {
    target_address_ = Address(config.target_endpoint);
    readout_window_ = config.readout_window;
    readout_window_.lookback_mode = to_lower_copy(readout_window_.lookback_mode);
    trigger_ = config.trigger;
    trigger_.mode = to_lower_copy(trigger_.mode);
    assign_dac_values_ = config.assign_dac_values;
    window_level_control_ = config.window_level_control;

    channel_settings_ = config.channels;
    enabled_channels_.clear();
    trigger_values_.clear();
    dac_values_.clear();

    for (const auto& [channel, channel_config] : channel_settings_) {
        trigger_values_.push_back(channel_config.trigger_value);
        dac_values_.push_back(channel_config.dac_value);
        if (channel_config.enabled) {
            enabled_channels_.push_back(channel);
        }
    }
}

}  // namespace nalu_board_controller
