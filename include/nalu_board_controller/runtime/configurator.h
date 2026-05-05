#ifndef NALU_BOARD_CONTROLLER_RUNTIME_CONFIGURATOR_H
#define NALU_BOARD_CONTROLLER_RUNTIME_CONFIGURATOR_H

#include "nalu_board_controller/runtime/state.h"

namespace nalu_board_controller {

class PythonBackend;

#if defined(__GNUC__)
#define NALU_BOARD_CONTROLLER_HIDDEN __attribute__((visibility("hidden")))
#else
#define NALU_BOARD_CONTROLLER_HIDDEN
#endif

class NALU_BOARD_CONTROLLER_HIDDEN Configurator {
public:
    Configurator(State* state, PythonBackend* python_backend);

    void configure_for_capture();

private:
    void configure_board_mode();
    void configure_triggers();
    void configure_dac_values();
    void configure_readout();
    void configure_connection();

    State* state_;
    PythonBackend* python_backend_;
};

#undef NALU_BOARD_CONTROLLER_HIDDEN

}  // namespace nalu_board_controller

#endif
