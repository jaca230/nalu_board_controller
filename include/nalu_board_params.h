#ifndef NALU_BOARD_PARAMS_H
#define NALU_BOARD_PARAMS_H

#include <string>

struct NaluBoardParams {
    std::string model = "HDSOCv1_evalr2";
    std::string board_ip_port = "192.168.1.59:4660";
    std::string host_ip_port = "192.168.1.1:4660";
    std::string config_file = "";
    std::string clock_file = "";
    bool debug = false;
};

#endif // NALU_BOARD_PARAMS_H
