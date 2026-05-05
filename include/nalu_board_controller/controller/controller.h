#ifndef NALU_BOARD_CONTROLLER_CONTROLLER_CONTROLLER_H
#define NALU_BOARD_CONTROLLER_CONTROLLER_CONTROLLER_H

#include <memory>
#include <string>
#include <vector>

#include "nalu_board_controller/config/board_config.h"
#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/readout_window_config.h"
#include "nalu_board_controller/config/trigger_config.h"
#include "nalu_board_controller/config/window_level_control_config.h"

namespace nalu_board_controller {

class Configurator;
class PythonBackend;
class State;

class Controller {
public:
    explicit Controller(const BoardConfig& config);
    ~Controller();

    void sync_python_logging(int level = 20);
    void sync_python_logging(const std::string& level);
    void initialize_board();

    void start_capture(const CaptureConfig& config);
    void start_capture(const std::string& target_endpoint,
                       const std::vector<int>& channels,
                       const ReadoutWindowConfig& readout_window,
                       const TriggerConfig& trigger,
                       const std::vector<int>& trigger_values,
                       const std::vector<int>& dac_values,
                       const WindowLevelControlConfig& window_level_control = {});
    void stop_capture();

    void enable_ethernet();
    void enable_serial();

private:
    void init_capture(const CaptureConfig& config);

    std::unique_ptr<State> state_;
    std::unique_ptr<PythonBackend> python_backend_;
    std::unique_ptr<Configurator> configurator_;
};

}  // namespace nalu_board_controller

#endif
