#include "nalu_board_controller.h"
#include "nalu_board_controller_logger.h"

NaluBoardController::NaluBoardController(const NaluBoardParams& params) {
    state_ = std::make_unique<NaluBoardState>(params);
    python_wrapper_ = std::make_unique<NaluBoardPythonWrapper>(state_.get());
    configurator_ = std::make_unique<NaluBoardConfigurator>(state_.get(), python_wrapper_.get());
}

NaluBoardController::~NaluBoardController() = default;

void NaluBoardController::setup_logger(int level) {
    python_wrapper_->SetupLogger(level);
}

void NaluBoardController::initialize_board() {
    python_wrapper_->InitializeBoard();
    state_->SetInitialized(true);
}

void NaluBoardController::start_capture(const NaluCaptureParams& params) {
    init_capture(params);
    python_wrapper_->StartCapture();
}

void NaluBoardController::start_capture(const std::string& target_ip_port,
                                     const std::vector<int>& channels,
                                     int windows, int lookback, int write_after_trig,
                                     const std::string& trigger_mode,
                                     const std::string& lookback_mode,
                                     const std::vector<int>& trigger_values,
                                     const std::vector<int>& dac_values,
                                     int low_reference, int high_reference,
                                     bool rising_edge) {
    NaluCaptureParams params;
    params.target_ip_port = target_ip_port;
    params.channels = {}; // Need to populate from channels vector
    params.windows = windows;
    params.lookback = lookback;
    params.write_after_trig = write_after_trig;
    params.trigger_mode = trigger_mode;
    params.lookback_mode = lookback_mode;
    params.low_reference = low_reference;
    params.high_reference = high_reference;
    params.rising_edge = rising_edge;
    
    init_capture(params);
    python_wrapper_->StartCapture();
}

void NaluBoardController::stop_capture() {
    python_wrapper_->StopCapture();
}

void NaluBoardController::enable_ethernet() {
    python_wrapper_->EnableEthernet();
}

void NaluBoardController::enable_serial() {
    python_wrapper_->EnableSerial();
}

void NaluBoardController::init_capture(const NaluCaptureParams& params) {
    if (!state_->IsInitialized()) {
        NaluBoardControllerLogger::error("Board not initialized. Call initialize_board() first.");
        throw std::runtime_error("Board not initialized");
    }
    
    state_->UpdateFromCaptureParams(params);
    configurator_->ConfigureForCapture();
}