#ifndef NALU_BOARD_CONFIGURATOR_H
#define NALU_BOARD_CONFIGURATOR_H

#include "nalu_board_state.h"
#include "nalu_board_python_wrapper.h"

class NaluBoardConfigurator {
public:
    NaluBoardConfigurator(NaluBoardState* state, NaluBoardPythonWrapper* python_wrapper);
    
    void ConfigureForCapture();

private:
    void ConfigureTriggers();
    void ConfigureDacValues();
    void ConfigureReadoutController();
    void ConfigureConnection();

    NaluBoardState* state_;
    NaluBoardPythonWrapper* python_wrapper_;
};

#endif // NALU_BOARD_CONFIGURATOR_H