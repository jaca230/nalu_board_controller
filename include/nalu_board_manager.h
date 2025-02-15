#ifndef NALU_BOARD_MANAGER_H
#define NALU_BOARD_MANAGER_H

#include <pybind11/embed.h>  
#include <string>
#include <vector>
#include <tuple>
#include "nalu_board_controller_logger.h"
#include "ip_address_info.h"

namespace py = pybind11;

class NaluBoardManager {
public:
    NaluBoardManager(const std::string& model, const std::string& board_ip_port,
                     const std::string& host_ip_port, const std::string& config_file = "",
                     const std::string& clock_file = "", bool debug = false);
    
    ~NaluBoardManager(); // Destructor

    void setup_logger(int level = 10);
    void initialize_board();
    
    // New init_capture method to set parameters
    void init_capture(const std::string& target_ip_port, const std::vector<int>& channels = {},
                      int windows = 8, int lookback = 8, int write_after_trig = 8, const std::string& trigger_mode = "ext", 
                      const std::string& lookback_mode = "", const std::vector<int>& trigger_values = {},
                      const std::vector<int>& dac_values = {});
    
    // start_capture method (with and without parameters)
    void start_capture();  // Without parameters
    void start_capture(const std::string& target_ip_port, const std::vector<int>& channels = {},
                       int windows = 8, int lookback = 8, int write_after_trig = 8, const std::string& trigger_mode = "ext", 
                       const std::string& lookback_mode = "", const std::vector<int>& trigger_values = {},
                       const std::vector<int>& dac_values = {});  // With parameters
    
    void stop_capture();
    void configure_triggers(const std::vector<int>& trigger_values, bool rising_edge = true);
    void enable_ethernet();
    void enable_serial();

private:
    // Parameters to establish connection to board
    std::string model;
    IPAddressInfo board_ip;  // board_ip is a pair of string and int
    IPAddressInfo host_ip;   // host_ip is also a pair

    // Optional parameters on connection
    std::string config_file;
    std::string clock_file;
    bool debug;
    
    //Parameters for starting aquisitions
    IPAddressInfo target_ip; 
    std::tuple<int, int, int> readout_window;
    std::string trigger_mode;
    std::string lookback_mode;
    std::vector<int> channels;
    std::vector<int> trigger_values;
    std::vector<int> dac_values;

    // Pybind11 objects
    py::object board;
    py::object board_controller;
    py::object control_registers;
    py::object analog_registers;
    py::object digital_registers;
    py::object trigger_controller;
    py::object data_collector;
    py::object logger;
};

#endif // NALU_BOARD_MANAGER_H
