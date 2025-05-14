#ifndef NALU_BOARD_CONTROLLER_H
#define NALU_BOARD_CONTROLLER_H

#include <pybind11/embed.h>  
#include <string>
#include <vector>
#include <tuple>
#include "nalu_board_controller_logger.h"
#include "ip_address_info.h"
#include "nalu_board_controller_params.h"

namespace py = pybind11;

class NaluBoardController {
public:
    NaluBoardController(const std::string& model, const std::string& board_ip_port,
                        const std::string& host_ip_port, const std::string& config_file = "",
                        const std::string& clock_file = "");
    NaluBoardController(const NaluBoardParams& params);
    
    ~NaluBoardController(); // Destructor

    void setup_logger(int level = 10);
    void initialize_board();
    
    void start_capture(const NaluCaptureParams& params);
    
    void stop_capture();
    void configure_triggers(const std::vector<int>& trigger_values, bool rising_edge = true);
    void enable_ethernet();
    void enable_serial();

private:
    void init_capture(const std::string& target_ip_port, const std::vector<int>& channels = {},
        int windows = 8, int lookback = 8, int write_after_trig = 8,
        const std::string& trigger_mode = "ext", const std::string& lookback_mode = "",
        const std::vector<int>& trigger_values = {}, const std::vector<int>& dac_values = {},
        int low_reference = 0, int high_reference = 15);

    void start_capture();
    void start_capture(const std::string& target_ip_port, const std::vector<int>& channels = {},
        int windows = 8, int lookback = 8, int write_after_trig = 8,
        const std::string& trigger_mode = "ext", const std::string& lookback_mode = "",
        const std::vector<int>& trigger_values = {}, const std::vector<int>& dac_values = {},
        int low_reference = 0, int high_reference = 15);

    std::string model;
    IPAddressInfo board_ip;
    IPAddressInfo host_ip;

    std::string config_file;
    std::string clock_file;
    
    IPAddressInfo target_ip; 
    std::tuple<int, int, int> readout_window;
    std::string trigger_mode;
    std::string lookback_mode;
    std::vector<int> channels;
    std::vector<int> trigger_values;
    std::vector<int> dac_values;
    int high_reference;
    int low_reference;

    py::object board;
    py::object board_controller;
    py::object control_registers;
    py::object analog_registers;
    py::object digital_registers;
    py::object trigger_controller;
    py::object data_collector;
    py::object logger;
};

#endif // NALU_BOARD_CONTROLLER_H
