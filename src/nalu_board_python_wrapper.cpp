#include "nalu_board_python_wrapper.h"
#include "nalu_board_controller_logger.h"

NaluBoardPythonWrapper::NaluBoardPythonWrapper(NaluBoardState* state) : state_(state) {
    NaluBoardControllerLogger::debug("NaluBoardPythonWrapper constructor called");
    InitializePythonInterpreter();
    // Don't import modules here - wait until InitializeBoard()
}

NaluBoardPythonWrapper::~NaluBoardPythonWrapper() {
    NaluBoardControllerLogger::debug("NaluBoardPythonWrapper destructor called");
    FinalizePythonInterpreter();
}

void NaluBoardPythonWrapper::InitializePythonInterpreter() {
    if (!Py_IsInitialized()) {
        NaluBoardControllerLogger::debug("Initializing Python interpreter...");
        py::initialize_interpreter();
        NaluBoardControllerLogger::debug("Python interpreter initialized");
    } else {
        NaluBoardControllerLogger::debug("Python interpreter already initialized");
    }
}

void NaluBoardPythonWrapper::FinalizePythonInterpreter() {
    try {
        if (Py_IsInitialized()) {
            NaluBoardControllerLogger::debug("Finalizing Python interpreter...");
            
            // Clear Python objects first without acquiring GIL
            ClearPythonObjects();
            
            // Don't call py::finalize_interpreter() - let it clean up naturally
            // This avoids the threading issues with GIL state
            NaluBoardControllerLogger::debug("Python interpreter finalization deferred to natural cleanup");
        } else {
            NaluBoardControllerLogger::debug("Python interpreter was not initialized, skipping finalization");
        }
    } catch (...) {
        // Silently handle any exceptions during cleanup
        // Don't log here as logging might also use Python
    }
}

void NaluBoardPythonWrapper::ClearPythonObjects() {
    try {
        NaluBoardControllerLogger::debug("Clearing Python object references");
        // Clear all Python object references
        board_ = py::object();
        board_controller_ = py::object();
        trigger_controller_ = py::object();
        readout_controller_ = py::object();
        connection_controller_ = py::object();
        dac_controller_ = py::object();
        control_registers_ = py::object();
        analog_registers_ = py::object();
        logger_ = py::object();
        NaluBoardControllerLogger::debug("Python object references cleared");
    } catch (...) {
        NaluBoardControllerLogger::debug("Exception caught during ClearPythonObjects (ignored)");
    }
}

void NaluBoardPythonWrapper::InitializeBoard() {
    try {
        NaluBoardControllerLogger::debug("Initializing board and controllers...");

        // Import necessary modules
        py::module naludaq_board = py::module::import("naludaq.board");
        py::module naludaq_comm = py::module::import("naludaq.communication");
        NaluBoardControllerLogger::debug("Imported naludaq.board and naludaq.communication modules");

        // Create board object
        board_ = naludaq_board.attr("Board")(state_->Model());
        NaluBoardControllerLogger::debug("Created board object for model: " + state_->Model());

        // Establish UDP connection
        py::tuple board_ip_tuple = py::make_tuple(state_->BoardIp().getIp(), state_->BoardIp().getPort());
        py::tuple host_ip_tuple = py::make_tuple(state_->HostIp().getIp(), state_->HostIp().getPort());
        NaluBoardControllerLogger::debug("Setting up UDP connection: board IP " + state_->BoardIp().getIp() + ":" + std::to_string(state_->BoardIp().getPort()) +
                                         ", host IP " + state_->HostIp().getIp() + ":" + std::to_string(state_->HostIp().getPort()));
        board_.attr("get_udp_connection")(board_ip_tuple, host_ip_tuple);
        NaluBoardControllerLogger::debug("UDP connection established");

        // Initialize all controllers
        board_controller_ = naludaq_board.attr("get_board_controller")(board_);
        NaluBoardControllerLogger::debug("Board controller obtained, resetting board");
        board_controller_.attr("reset_board")();
        NaluBoardControllerLogger::debug("Board reset completed");

        trigger_controller_ = naludaq_board.attr("get_trigger_controller")(board_);
        readout_controller_ = naludaq_board.attr("get_readout_controller")(board_);
        dac_controller_ = naludaq_board.attr("get_dac_controller")(board_);
        NaluBoardControllerLogger::debug("Trigger, readout, and DAC controllers obtained");

        py::module naludaq_conn = py::module::import("naludaq.controllers");
        connection_controller_ = naludaq_conn.attr("get_connection_controller")(board_);
        NaluBoardControllerLogger::debug("Connection controller obtained");

        // Load configuration files if specified
        if (!state_->ClockFile().empty()) {
            NaluBoardControllerLogger::debug("Loading clock file: " + state_->ClockFile());
            board_.attr("load_clockfile")(state_->ClockFile());
            NaluBoardControllerLogger::debug("Clock file loaded");
        } else {
            NaluBoardControllerLogger::debug("No clock file specified");
        }

        if (!state_->ConfigFile().empty()) {
            NaluBoardControllerLogger::debug("Loading register config file: " + state_->ConfigFile());
            board_.attr("load_registers")(state_->ConfigFile());
            NaluBoardControllerLogger::debug("Register config file loaded");
        } else {
            NaluBoardControllerLogger::debug("No register config file specified");
        }

        // Initialize control registers
        control_registers_ = naludaq_comm.attr("ControlRegisters")(board_);
        analog_registers_ = naludaq_comm.attr("analog_registers").attr("AnalogRegisters")(board_);
        NaluBoardControllerLogger::debug("Control and analog registers initialized");

        // Complete board startup
        py::object startup_board = naludaq_board.attr("startup_board");
        startup_board(board_);
        NaluBoardControllerLogger::info("Board and controllers initialized successfully");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Board initialization failed: ") + e.what());
        throw;
    }
}

void NaluBoardPythonWrapper::SetupLogger(int level) {
    try {
        NaluBoardControllerLogger::debug("Setting up Python logging with level " + std::to_string(level));
        py::module logging = py::module::import("logging");
        logger_ = logging.attr("getLogger")();
        py::object handler = logging.attr("StreamHandler")();
        handler.attr("setFormatter")(logging.attr("Formatter")("%(asctime)s %(name)-30s [%(levelname)-6s]: %(message)s"));
        logger_.attr("addHandler")(handler);
        logger_.attr("setLevel")(level);
        NaluBoardControllerLogger::debug("Logger handler added and level set");

        py::list suppress = py::cast(std::vector<std::string>{"naludaq.UART", "naludaq.FTDI"});
        for (auto& name : suppress) {
            logging.attr("getLogger")(name).attr("setLevel")(10);  // DEBUG
            NaluBoardControllerLogger::debug("Suppressed logging level to DEBUG for: " + std::string(py::str(name)));
        }
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Logger setup error: ") + e.what());
        throw;
    }
}

void NaluBoardPythonWrapper::StartCapture() {
    try {
        NaluBoardControllerLogger::debug("Starting capture...");
        if (!board_ || board_.is_none()) {
            throw std::runtime_error("Board not initialized. Call InitializeBoard() first.");
        }

        // Configure target IP
        py::object target_ip_tuple = py::make_tuple(state_->TargetIp().getIp(), state_->TargetIp().getPort());
        NaluBoardControllerLogger::debug("Setting target IP to " + state_->TargetIp().getIp() + ":" + std::to_string(state_->TargetIp().getPort()));
        py::object connection_info = board_.attr("connection_info");
        connection_info.attr("__setitem__")("receiver_addr", target_ip_tuple);

        // Configure Ethernet
        NaluBoardControllerLogger::debug("Configuring Ethernet connection...");
        connection_controller_.attr("_configure_ethernet")();
        NaluBoardControllerLogger::debug("Ethernet configured");

        // Start readout
        if (!state_->LookbackMode().empty()) {
            NaluBoardControllerLogger::debug("Starting readout with trigger mode: " + state_->TriggerMode() + ", lookback mode: " + state_->LookbackMode());
            board_controller_.attr("start_readout")(
                py::str(state_->TriggerMode()),
                py::str(state_->LookbackMode())
            );
        } else {
            NaluBoardControllerLogger::debug("Starting readout with trigger mode: " + state_->TriggerMode());
            board_controller_.attr("start_readout")(py::str(state_->TriggerMode()));
        }

        NaluBoardControllerLogger::info("Capture started successfully");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Start capture failed: ") + e.what());
        throw;
    }
}

void NaluBoardPythonWrapper::StopCapture() {
    try {
        NaluBoardControllerLogger::debug("Stopping capture...");
        if (board_controller_ && !board_controller_.is_none()) {
            board_controller_.attr("stop_readout")();
            NaluBoardControllerLogger::info("Capture stopped successfully");
        } else {
            NaluBoardControllerLogger::debug("Board controller is not initialized or is None, nothing to stop");
        }
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Stop capture error: ") + e.what());
        throw;
    }
}

void NaluBoardPythonWrapper::EnableEthernet() {
    try {
        NaluBoardControllerLogger::debug("Enabling Ethernet mode...");
        py::module naludaq_communication = py::module::import("naludaq.communication");
        py::object control_registers = naludaq_communication.attr("ControlRegisters")(board_);
        control_registers.attr("write")("iomode0", true);   // Disable serial
        control_registers.attr("write")("iomode1", false);  // Enable Ethernet
        NaluBoardControllerLogger::info("Ethernet mode enabled");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Enable Ethernet error: ") + e.what());
        throw;
    }
}

void NaluBoardPythonWrapper::EnableSerial() {
    try {
        NaluBoardControllerLogger::debug("Enabling Serial mode...");
        py::module naludaq_communication = py::module::import("naludaq.communication");
        py::object control_registers = naludaq_communication.attr("ControlRegisters")(board_);
        control_registers.attr("write")("iomode0", false);  // Enable serial
        control_registers.attr("write")("iomode1", true);   // Disable Ethernet
        NaluBoardControllerLogger::info("Serial mode enabled");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Enable Serial error: ") + e.what());
        throw;
    }
}
