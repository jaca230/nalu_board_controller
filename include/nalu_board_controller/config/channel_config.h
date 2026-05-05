#ifndef NALU_BOARD_CONTROLLER_CONFIG_CHANNEL_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_CHANNEL_CONFIG_H

namespace nalu_board_controller {

struct ChannelConfig {
    bool enabled = true;
    int trigger_value = 0;
    int dac_value = 1804;
};

}  // namespace nalu_board_controller

#endif
