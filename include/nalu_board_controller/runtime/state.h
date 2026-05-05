#ifndef NALU_BOARD_CONTROLLER_RUNTIME_STATE_H
#define NALU_BOARD_CONTROLLER_RUNTIME_STATE_H

#include <map>
#include <string>
#include <vector>

#include "nalu_board_controller/config/board_config.h"
#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/channel_config.h"
#include "nalu_board_controller/config/readout_window_config.h"
#include "nalu_board_controller/config/trigger_config.h"
#include "nalu_board_controller/config/window_level_control_config.h"
#include "nalu_board_controller/types/address.h"

namespace nalu_board_controller {

class State {
public:
    explicit State(const BoardConfig& config);

    bool is_initialized() const { return is_initialized_; }
    void set_initialized(bool initialized) { is_initialized_ = initialized; }

    const std::string& model() const { return model_; }
    const Address& board_address() const { return board_address_; }
    const Address& host_address() const { return host_address_; }
    const std::string& config_file() const { return config_file_; }
    const std::string& clock_file() const { return clock_file_; }

    const Address& target_address() const { return target_address_; }
    const ReadoutWindowConfig& readout_window() const { return readout_window_; }
    const TriggerConfig& trigger() const { return trigger_; }
    const std::vector<int>& enabled_channels() const { return enabled_channels_; }
    const std::map<int, ChannelConfig>& channel_settings() const { return channel_settings_; }
    const std::vector<int>& trigger_values() const { return trigger_values_; }
    const std::vector<int>& dac_values() const { return dac_values_; }
    bool assign_dac_values() const { return assign_dac_values_; }
    const WindowLevelControlConfig& window_level_control() const { return window_level_control_; }

    void update_from_capture_config(const CaptureConfig& config);

private:
    bool is_initialized_ = false;
    std::string model_;
    Address board_address_;
    Address host_address_;
    std::string config_file_;
    std::string clock_file_;

    Address target_address_;
    ReadoutWindowConfig readout_window_;
    TriggerConfig trigger_;
    std::map<int, ChannelConfig> channel_settings_;
    std::vector<int> enabled_channels_;
    std::vector<int> trigger_values_;
    std::vector<int> dac_values_;
    bool assign_dac_values_ = false;
    WindowLevelControlConfig window_level_control_;
};

}  // namespace nalu_board_controller

#endif
