#ifndef NALU_BOARD_CONTROLLER_CONFIG_BOARD_CONFIG_H
#define NALU_BOARD_CONTROLLER_CONFIG_BOARD_CONFIG_H

#include <string>

namespace nalu_board_controller {

struct BoardConfig {
    std::string model = "HDSoCv1_evalr2";
    std::string board_endpoint = "192.168.1.59:4660";
    std::string host_endpoint = "192.168.1.1:4660";
    std::string config_file;
    std::string clock_file;
};

}  // namespace nalu_board_controller

#endif
