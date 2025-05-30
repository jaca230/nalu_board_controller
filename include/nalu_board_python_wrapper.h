#ifndef NALU_BOARD_PYTHON_WRAPPER_H
#define NALU_BOARD_PYTHON_WRAPPER_H

#include <pybind11/embed.h>
#include "nalu_board_state.h"

namespace py = pybind11;

class NaluBoardPythonWrapper {
public:
    explicit NaluBoardPythonWrapper(NaluBoardState* state);
    ~NaluBoardPythonWrapper();

    void SetupLogger(int level);
    void InitializeBoard();
    void StartCapture();
    void StopCapture();
    void EnableEthernet();
    void EnableSerial();

    // Controller accessors
    py::object& Board() { return board_; }
    py::object& BoardController() { return board_controller_; }
    py::object& TriggerController() { return trigger_controller_; }
    py::object& ReadoutController() { return readout_controller_; }
    py::object& ConnectionController() { return connection_controller_; }
    py::object& DacController() { return dac_controller_; }
    py::object& Logger() { return logger_; }

    // Finalize the Python interpreter
    void FinalizePythonInterpreter();
    void ClearPythonObjects();

private:
    void InitializePythonInterpreter();
    void ImportPythonModules();
    void SetupBoardConnection();

    NaluBoardState* state_;
    py::object board_;
    py::object board_controller_;
    py::object trigger_controller_;
    py::object readout_controller_;
    py::object connection_controller_;
    py::object dac_controller_;
    py::object control_registers_;
    py::object analog_registers_;
    py::object logger_;
};

#endif // NALU_BOARD_PYTHON_WRAPPER_H