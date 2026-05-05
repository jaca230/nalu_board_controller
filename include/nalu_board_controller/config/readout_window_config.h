#ifndef NALU_BOARD_CONTROLLER_CONFIG_READOUT_WINDOW_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_READOUT_WINDOW_CONFIG_H

#include <string>

namespace nalu_board_controller {

struct ReadoutWindowConfig {
    int windows = 1;
    int lookback = 1;
    int write_after_trigger = 1;
    std::string lookback_mode = "default";
};

}  // namespace nalu_board_controller

#endif
