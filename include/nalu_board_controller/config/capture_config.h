#ifndef NALU_BOARD_CONTROLLER_CONFIG_CAPTURE_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_CAPTURE_CONFIG_H

#include <map>
#include <string>

#include "nalu_board_controller/config/channel_config.h"
#include "nalu_board_controller/config/readout_window_config.h"
#include "nalu_board_controller/config/trigger_config.h"
#include "nalu_board_controller/config/window_level_control_config.h"

namespace nalu_board_controller {

struct CaptureConfig {
    std::string target_endpoint = "192.168.1.1:12345";
    bool assign_dac_values = false;
    ReadoutWindowConfig readout_window;
    TriggerConfig trigger;
    WindowLevelControlConfig window_level_control;
    std::map<int, ChannelConfig> channels;
};

}  // namespace nalu_board_controller

#endif
