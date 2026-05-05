#include "nalu_board_controller/python/backend.h"

#include <stdexcept>

#include <spdlog/spdlog.h>

#include "nalu_board_controller/logging/logging.h"
#include "nalu_board_controller/runtime/state.h"

namespace nalu_board_controller {

namespace {

void set_python_root_level(py::module_& logging, const py::object& logger, const std::string& level) {
    py::object py_level = logging.attr(level == "warning" ? "WARNING" :
                                       level == "error" ? "ERROR" :
                                       level == "debug" ? "DEBUG" : "INFO");
    logger.attr("setLevel")(py_level);
}

}  // namespace

PythonBackend::PythonBackend(State* state) : state_(state) {
    initialize_interpreter();
}

PythonBackend::~PythonBackend() {
    finalize_interpreter();
}

void PythonBackend::initialize_interpreter() {
    if (!Py_IsInitialized()) {
        spdlog::debug("Initializing embedded Python interpreter");
        py::initialize_interpreter();
    }
}

void PythonBackend::finalize_interpreter() {
    try {
        if (Py_IsInitialized()) {
            clear_python_objects();
        }
    } catch (...) {
    }
}

void PythonBackend::clear_python_objects() {
    board_ = py::object();
    board_controller_ = py::object();
    trigger_controller_ = py::object();
    readout_controller_ = py::object();
    connection_controller_ = py::object();
    dac_controller_ = py::object();
    control_registers_ = py::object();
    analog_registers_ = py::object();
    digital_registers_ = py::object();
}

void PythonBackend::sync_python_logging(int level) {
    spdlog::set_level(logging::level_from_python(level));
    try {
        py::module_ logging_module = py::module_::import("logging");
        py::object logger = logging_module.attr("getLogger")();
        if (py::len(logger.attr("handlers")) == 0) {
            py::object handler = logging_module.attr("StreamHandler")();
            handler.attr("setFormatter")(
                logging_module.attr("Formatter")("%(asctime)s %(name)-30s [%(levelname)-6s]: %(message)s"));
            logger.attr("addHandler")(handler);
        }
        logger.attr("setLevel")(level);
    } catch (const py::error_already_set& error) {
        spdlog::warn("Python logger setup failed: {}", error.what());
    }
}

void PythonBackend::sync_python_logging(const std::string& level) {
    spdlog::set_level(spdlog::level::from_str(level));
    try {
        py::module_ logging_module = py::module_::import("logging");
        py::object logger = logging_module.attr("getLogger")();
        if (py::len(logger.attr("handlers")) == 0) {
            py::object handler = logging_module.attr("StreamHandler")();
            handler.attr("setFormatter")(
                logging_module.attr("Formatter")("%(asctime)s %(name)-30s [%(levelname)-6s]: %(message)s"));
            logger.attr("addHandler")(handler);
        }
        set_python_root_level(logging_module, logger, level);
    } catch (const py::error_already_set& error) {
        spdlog::warn("Python logger setup failed: {}", error.what());
    }
}

void PythonBackend::initialize_board() {
    try {
        py::module_ naludaq_board = py::module_::import("naludaq.board");
        py::module_ naludaq_comm = py::module_::import("naludaq.communication");
        py::module_ naludaq_conn = py::module_::import("naludaq.controllers");

        board_ = naludaq_board.attr("Board")(state_->model());
        board_.attr("get_udp_connection")(
            py::make_tuple(state_->board_address().getIp(), state_->board_address().getPort()),
            py::make_tuple(state_->host_address().getIp(), state_->host_address().getPort()));

        board_.attr("params").attr("__setitem__")("serial_mode", py::bool_(state_->asic_serial_mode()));
        spdlog::info("Using startup settings: asic_serial_mode={}, clock_file={}",
                     state_->asic_serial_mode(),
                     state_->clock_file().empty() ? std::string("<default>") : state_->clock_file());

        board_controller_ = naludaq_board.attr("get_board_controller")(board_);
        trigger_controller_ = naludaq_board.attr("get_trigger_controller")(board_);
        readout_controller_ = naludaq_board.attr("get_readout_controller")(board_);
        dac_controller_ = naludaq_board.attr("get_dac_controller")(board_);
        connection_controller_ = naludaq_conn.attr("get_connection_controller")(board_);

        board_controller_.attr("reset_board")();

        if (!state_->clock_file().empty()) {
            board_.attr("load_clockfile")(state_->clock_file());
        }
        if (!state_->config_file().empty()) {
            board_.attr("load_registers")(state_->config_file());
        }

        control_registers_ = naludaq_comm.attr("ControlRegisters")(board_);
        analog_registers_ = naludaq_comm.attr("AnalogRegisters")(board_);
        digital_registers_ = naludaq_comm.attr("DigitalRegisters")(board_);

        naludaq_board.attr("startup_board")(board_);
        spdlog::info("Board '{}' initialized successfully", state_->model());
    } catch (const py::error_already_set& error) {
        spdlog::error("Board initialization failed: {}", error.what());
        throw;
    }
}

void PythonBackend::restart_board() {
    try {
        py::module_ naludaq_board = py::module_::import("naludaq.board");
        board_controller_.attr("reset_board")();
        naludaq_board.attr("startup_board")(board_);
        spdlog::debug("Board reinitialized from current register state");
    } catch (const py::error_already_set& error) {
        spdlog::error("Board reinitialization failed: {}", error.what());
        throw;
    }
}

void PythonBackend::apply_wlc(const WindowLevelControlConfig& config) {
    try {
        if (!digital_registers_ || digital_registers_.is_none()) {
            throw std::runtime_error("Digital register interface is not initialized");
        }

        digital_registers_.attr("write")("wlc_on", config.enabled ? 1 : 0);
        spdlog::debug("Digital register 'wlc_on' set to {}", config.enabled ? 1 : 0);

        if (config.reinitialize_after_change) {
            restart_board();
        }
    } catch (const py::error_already_set& error) {
        spdlog::error("Applying WLC configuration failed: {}", error.what());
        throw;
    }
}

void PythonBackend::start_capture() {
    try {
        if (!board_ || board_.is_none()) {
            throw std::runtime_error("Board not initialized. Call initialize_board() first.");
        }

        board_.attr("connection_info").attr("__setitem__")(
            "receiver_addr",
            py::make_tuple(state_->target_address().getIp(), state_->target_address().getPort()));

        connection_controller_.attr("_configure_ethernet")();

        const auto& readout_window = state_->readout_window();
        if (!readout_window.lookback_mode.empty() && readout_window.lookback_mode != "default") {
            board_controller_.attr("start_readout")(
                py::arg("trig") = py::str(state_->trigger().mode),
                py::arg("lb") = py::str(readout_window.lookback_mode));
        } else {
            board_controller_.attr("start_readout")(py::arg("trig") = py::str(state_->trigger().mode));
        }

        spdlog::info("Capture started successfully");
    } catch (const py::error_already_set& error) {
        spdlog::error("Start capture failed: {}", error.what());
        throw;
    }
}

void PythonBackend::stop_capture() {
    try {
        if (board_controller_ && !board_controller_.is_none()) {
            board_controller_.attr("stop_readout")();
            spdlog::info("Capture stopped successfully");
        }
    } catch (const py::error_already_set& error) {
        spdlog::error("Stop capture failed: {}", error.what());
        throw;
    }
}

void PythonBackend::enable_ethernet() {
    try {
        control_registers_.attr("write")("iomode0", true);
        control_registers_.attr("write")("iomode1", false);
        spdlog::info("Ethernet mode enabled");
    } catch (const py::error_already_set& error) {
        spdlog::error("Enable Ethernet failed: {}", error.what());
        throw;
    }
}

void PythonBackend::enable_serial() {
    try {
        control_registers_.attr("write")("iomode0", false);
        control_registers_.attr("write")("iomode1", true);
        spdlog::info("Serial mode enabled");
    } catch (const py::error_already_set& error) {
        spdlog::error("Enable Serial failed: {}", error.what());
        throw;
    }
}

}  // namespace nalu_board_controller
