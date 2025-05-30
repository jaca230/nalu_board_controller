#ifndef NALU_BOARD_CONTROLLER_H
#define NALU_BOARD_CONTROLLER_H

#include <memory>
#include <string>
#include <vector>
#include "nalu_board_controller_params.h"
#include "nalu_board_state.h"
#include "nalu_board_python_wrapper.h"
#include "nalu_board_configurator.h"

class NaluBoardController {
public:
    explicit NaluBoardController(const NaluBoardParams& params);
    ~NaluBoardController();

    void setup_logger(int level = 20);  // Default to INFO level
    void initialize_board();
    
    void start_capture(const NaluCaptureParams& params);
    void start_capture(const std::string& target_ip_port, 
                     const std::vector<int>& channels,
                     int windows, int lookback, int write_after_trig,
                     const std::string& trigger_mode,
                     const std::string& lookback_mode,
                     const std::vector<int>& trigger_values,
                     const std::vector<int>& dac_values,
                     int low_reference, int high_reference,
                     bool rising_edge);
    void stop_capture();

    void enable_ethernet();
    void enable_serial();

private:
    void init_capture(const NaluCaptureParams& params);

    std::unique_ptr<NaluBoardState> state_;
    std::unique_ptr<NaluBoardPythonWrapper> python_wrapper_;
    std::unique_ptr<NaluBoardConfigurator> configurator_;
};

#endif // NALU_BOARD_CONTROLLER_H