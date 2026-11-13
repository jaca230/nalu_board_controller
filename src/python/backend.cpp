#include "nalu_board_controller/python/backend.h"

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>

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

void ping_board(const std::string& ip) {
    try {
        py::module_ subprocess = py::module_::import("subprocess");
        subprocess.attr("run")(
            py::make_tuple("ping", "-c", "1", "-W", "1", ip),
            py::arg("stdout") = subprocess.attr("DEVNULL"),
            py::arg("stderr") = subprocess.attr("DEVNULL"),
            py::arg("check") = false);
    } catch (const py::error_already_set& error) {
        spdlog::debug("Could not ping board {}: {}", ip, error.what());
    }
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

bool PythonBackend::wait_for_identifier(int attempts) {
    py::object registers = py::module_::import("naludaq.communication").attr("ControlRegisters")(board_);
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        ping_board(state_->board_address().getIp());
        try {
            py::dict response = registers.attr("read")("identifier").cast<py::dict>();
            const int identifier = response["value"].cast<int>();
            if (identifier == 0x4D5C) {
                spdlog::info("Board identifier verified: {} (0x4D5C)", identifier);
                return true;
            }
            spdlog::debug("Identifier attempt {}/{} returned {}", attempt, attempts, identifier);
        } catch (const py::error_already_set& error) {
            spdlog::debug("Identifier attempt {}/{} failed: {}", attempt, attempts, error.what());
        }
        if (attempt < attempts) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    return false;
}

bool PythonBackend::connect_verified_udp_board(bool fresh_board) {
    py::module_ naludaq_board = py::module_::import("naludaq.board");
    constexpr int connection_attempts = 3;
    for (int attempt = 1; attempt <= connection_attempts; ++attempt) {
        if (fresh_board) {
            board_ = naludaq_board.attr("Board")(state_->model());
        } else {
            board_.attr("disconnect")();
        }
        ping_board(state_->board_address().getIp());
        try {
            board_.attr("get_udp_connection")(
                py::make_tuple(state_->board_address().getIp(), state_->board_address().getPort()),
                py::make_tuple(state_->host_address().getIp(), state_->host_address().getPort()));
            // naludaq's read_until has no datagram limit. A stale event stream on
            // the control port can therefore keep a register read inside recv
            // forever. Preserve its stop-word protocol while bounding the read.
            py::object connection = board_.attr("connection");
            connection.attr("read_until") = py::cpp_function(
                [connection](py::bytes stop_word) -> py::bytes {
                    const std::string stop = stop_word;
                    std::string buffer;
                    constexpr int max_datagrams = 64;
                    for (int packet = 0; packet < max_datagrams; ++packet) {
                        const std::string next = py::bytes(connection.attr("read")(24));
                        if (next.empty()) {
                            break;
                        }
                        buffer += next;
                        if (!stop.empty() && buffer.size() >= stop.size() &&
                            buffer.compare(buffer.size() - stop.size(), stop.size(), stop) == 0) {
                            break;
                        }
                    }
                    return py::bytes(buffer);
                });
            // A previous process may have exited without stopping readout. Reconnecting
            // routes that stream to the control socket, where it can starve register reads.
            naludaq_board.attr("get_board_controller")(board_).attr("stop_readout")();
            if (wait_for_identifier(fresh_board ? 2 : 5)) {
                return false;
            }
            if (fresh_board) {
                spdlog::warn("Identifier invalid after UDP connection; resetting and starting the board before retrying");
                board_.attr("params").attr("__setitem__")(
                    "serial_mode", py::bool_(state_->asic_serial_mode()));
                naludaq_board.attr("get_board_controller")(board_).attr("reset_board")();
                startup_board_with_retries();
                if (wait_for_identifier(2)) {
                    return true;
                }
            }
        } catch (const py::error_already_set& error) {
            spdlog::warn("UDP connection attempt {}/{} failed: {}",
                         attempt, connection_attempts, error.what());
        }
        board_.attr("disconnect")();
        if (attempt < connection_attempts) {
            spdlog::warn("UDP connection attempt {}/{} returned no valid identifier; reconnecting",
                         attempt, connection_attempts);
        }
    }
    throw std::runtime_error("Board did not return identifier 19804 (0x4D5C) after UDP reconnects");
}

void PythonBackend::ensure_control_connection() {
    if (!board_ || board_.is_none()) {
        throw std::runtime_error("Board not initialized. Call initialize_board() first.");
    }
    if (!wait_for_identifier(1)) {
        spdlog::warn("UDP control connection is not responding before capture; reconnecting");
        connect_verified_udp_board(false);
    }
}

void PythonBackend::startup_board_with_retries() {
    py::module_ naludaq_board = py::module_::import("naludaq.board");
    py::list startup_status;
    constexpr int startup_attempts = 10;
    for (int attempt = 1; attempt <= startup_attempts; ++attempt) {
        startup_status = py::list();
        if (naludaq_board.attr("startup_board")(
                board_, py::arg("stat") = startup_status).cast<bool>()) {
            spdlog::info("startup_board succeeded on attempt {}/{}", attempt, startup_attempts);
            return;
        }
        spdlog::warn("startup_board attempt {}/{} returned false", attempt, startup_attempts);
        if (attempt < startup_attempts) {
            ping_board(state_->board_address().getIp());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    std::ostringstream message;
    message << "startup_board() returned false after " << startup_attempts << " attempts";
    for (const auto& entry : startup_status) {
        try {
            const py::tuple status = entry.cast<py::tuple>();
            if (py::len(status) >= 2) {
                message << " [" << py::str(status[0]).cast<std::string>() << "] "
                        << py::str(status[1]).cast<std::string>();
            }
        } catch (const py::cast_error&) {
        }
    }
    throw std::runtime_error(message.str());
}

void PythonBackend::initialize_board() {
    try {
        py::module_ naludaq = py::module_::import("naludaq");
        py::module_ naludaq_board = py::module_::import("naludaq.board");
        py::module_ naludaq_comm = py::module_::import("naludaq.communication");
        py::module_ naludaq_conn = py::module_::import("naludaq.controllers");

        const std::string naludaq_version = py::str(naludaq.attr("__version__"));
        const std::string naludaq_path = py::str(naludaq.attr("__file__"));
        spdlog::info("Using naludaq {} from {}", naludaq_version, naludaq_path);

        connect_verified_udp_board(true);

        board_.attr("params").attr("__setitem__")("serial_mode", py::bool_(state_->asic_serial_mode()));
        spdlog::info("Using startup settings: asic_serial_mode={}, clock_file={}",
                     state_->asic_serial_mode(),
                     state_->clock_file().empty() ? std::string("<default>") : state_->clock_file());

        board_controller_ = naludaq_board.attr("get_board_controller")(board_);
        trigger_controller_ = naludaq_board.attr("get_trigger_controller")(board_);
        readout_controller_ = naludaq_board.attr("get_readout_controller")(board_);
        dac_controller_ = naludaq_board.attr("get_dac_controller")(board_);
        connection_controller_ = naludaq_conn.attr("get_connection_controller")(board_);

        // Selecting WLC mode changes startup-sensitive firmware state, so board
        // initialization must always finish with a reset/startup cycle.
        const bool needs_startup = true;
        if (needs_startup) {
            board_controller_.attr("reset_board")();
        }

        if (!state_->clock_file().empty()) {
            board_.attr("load_clockfile")(state_->clock_file());
        }
        if (!state_->config_file().empty()) {
            board_.attr("load_registers")(state_->config_file());
        }

        control_registers_ = naludaq_comm.attr("ControlRegisters")(board_);
        analog_registers_ = naludaq_comm.attr("AnalogRegisters")(board_);
        digital_registers_ = naludaq_comm.attr("DigitalRegisters")(board_);

        const auto& window_level_control = state_->window_level_control();
        digital_registers_.attr("write")("wlc_on", window_level_control.enabled ? 1 : 0);
        spdlog::info("Preparing WLC mode during board initialization: enabled={}",
                     window_level_control.enabled);

        if (needs_startup) {
            startup_board_with_retries();
        } else {
            spdlog::info("Using board startup completed during UDP connection recovery");
        }
        spdlog::info("Board '{}' initialized successfully", state_->model());
    } catch (const py::error_already_set& error) {
        spdlog::error("Board initialization failed: {}", error.what());
        throw;
    }
}

void PythonBackend::restart_board() {
    try {
        board_controller_.attr("reset_board")();
        startup_board_with_retries();
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

        // Firmware 959 needs time to apply the new UDP destination before
        // it will reliably accept the readout-start command.
        constexpr auto receiver_settle_delay = std::chrono::seconds(2);
        spdlog::info("Waiting {} seconds for UDP destination to settle before readout",
                     receiver_settle_delay.count());
        std::this_thread::sleep_for(receiver_settle_delay);

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

std::string PythonBackend::read_firmware_version() const {
    try {
        if (!board_controller_ || board_controller_.is_none()) {
            throw std::runtime_error("Board controller is not initialized");
        }
        return py::str(board_controller_.attr("read_firmware_version")()).cast<std::string>();
    } catch (const py::error_already_set& error) {
        spdlog::error("Read firmware version failed: {}", error.what());
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
