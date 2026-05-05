#ifndef NALU_BOARD_CONTROLLER_CONFIG_TRIGGER_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_TRIGGER_CONFIG_H

#include <string>

namespace nalu_board_controller {

struct TriggerConfig {
    std::string mode = "ext";
    int low_reference = 0;
    int high_reference = 15;
    bool rising_edge = true;
};

}  // namespace nalu_board_controller

#endif
