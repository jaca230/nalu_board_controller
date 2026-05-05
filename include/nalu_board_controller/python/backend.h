#ifndef NALU_BOARD_CONTROLLER_PYTHON_BACKEND_H
#define NALU_BOARD_CONTROLLER_PYTHON_BACKEND_H

#include <pybind11/embed.h>

#include "nalu_board_controller/config/window_level_control_config.h"

namespace py = pybind11;
namespace nalu_board_controller {

class State;

#if defined(__GNUC__)
#define NALU_BOARD_CONTROLLER_HIDDEN __attribute__((visibility("hidden")))
#else
#define NALU_BOARD_CONTROLLER_HIDDEN
#endif

class NALU_BOARD_CONTROLLER_HIDDEN PythonBackend {
public:
    explicit PythonBackend(State* state);
    ~PythonBackend();

    void sync_python_logging(int level);
    void sync_python_logging(const std::string& level);
    void initialize_board();
    void start_capture();
    void stop_capture();
    void enable_ethernet();
    void enable_serial();
    void apply_wlc(const WindowLevelControlConfig& config);

    py::object& board() { return board_; }
    py::object& board_controller() { return board_controller_; }
    py::object& trigger_controller() { return trigger_controller_; }
    py::object& readout_controller() { return readout_controller_; }
    py::object& connection_controller() { return connection_controller_; }
    py::object& dac_controller() { return dac_controller_; }
    py::object& digital_registers() { return digital_registers_; }
    py::object& analog_registers() { return analog_registers_; }

private:
    void initialize_interpreter();
    void finalize_interpreter();
    void clear_python_objects();
    void restart_board();

    State* state_;
    py::object board_;
    py::object board_controller_;
    py::object trigger_controller_;
    py::object readout_controller_;
    py::object connection_controller_;
    py::object dac_controller_;
    py::object control_registers_;
    py::object analog_registers_;
    py::object digital_registers_;
};

#undef NALU_BOARD_CONTROLLER_HIDDEN

}  // namespace nalu_board_controller

#endif
