#ifndef NALU_BOARD_CONTROLLER_CONFIG_WINDOW_LEVEL_CONTROL_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_WINDOW_LEVEL_CONTROL_CONFIG_H

namespace nalu_board_controller {

struct WindowLevelControlConfig {
    bool configure = false;
    bool enabled = false;
    bool reinitialize_after_change = true;
};

}  // namespace nalu_board_controller

#endif
