#include "nalu_board_controller.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>  // For std::transform, std::find
#include <cctype>     // For std::tolower

NaluBoardController::NaluBoardController(const std::string& model, const std::string& board_ip_port, 
                                   const std::string& host_ip_port, const std::string& config_file, 
                                   const std::string& clock_file) 
    : config_file(config_file), clock_file(clock_file) {
    
    // Convert model to lowercase
    this->model = model;
    std::transform(this->model.begin(), this->model.end(), this->model.begin(), ::tolower);

    // Parse IP and port using IPHelpers
    this->board_ip = IPAddressInfo(board_ip_port);
    this->host_ip = IPAddressInfo(host_ip_port);

    NaluBoardControllerLogger::debug("Initializing Python interpreter...");
    py::initialize_interpreter();  // Initialize Python interpreter once
    NaluBoardControllerLogger::debug("Python interpreter initialized.");
}

NaluBoardController::NaluBoardController(const NaluBoardParams& params) 
    : config_file(params.config_file), clock_file(params.clock_file) {
    // There is some pybind11 tomfoolery that requires us to repeat this code instead of just calling
    // the other constructor. I have no idea why. My guess is something in pybind goes out of scope if I try
    // to call the other constructor. If we don't do this, the code segfaults when trying to import naludaq.
    
    // Convert model to lowercase
    this->model = params.model;
    std::transform(this->model.begin(), this->model.end(), this->model.begin(), ::tolower);

    // Parse IP and port using IPHelpers
    this->board_ip = IPAddressInfo(params.board_ip_port);
    this->host_ip = IPAddressInfo(params.host_ip_port);

    NaluBoardControllerLogger::debug("Initializing Python interpreter...");
    py::initialize_interpreter();  // Initialize Python interpreter once
    NaluBoardControllerLogger::debug("Python interpreter initialized.");
}


NaluBoardController::~NaluBoardController() {
    NaluBoardControllerLogger::debug("Cleaning up Python objects...");

    // Reset all Python objects to avoid crashes
    board = py::none();
    board_controller = py::none();
    control_registers = py::none();
    analog_registers = py::none();
    digital_registers = py::none();
    trigger_controller = py::none();
    data_collector = py::none();
    logger = py::none();

    NaluBoardControllerLogger::debug("Finalizing Python interpreter...");
    py::finalize_interpreter();
    NaluBoardControllerLogger::debug("Python interpreter finalized.");
}

void NaluBoardController::setup_logger(int level) {
    try {
        NaluBoardControllerLogger::debug("Setting up logger...");
        py::module logging = py::module::import("logging");
        logger = logging.attr("getLogger")();
        py::object handler = logging.attr("StreamHandler")();
        handler.attr("setFormatter")(logging.attr("Formatter")("%(asctime)s %(name)-30s [%(levelname)-6s]: %(message)s"));
        logger.attr("addHandler")(handler);
        logger.attr("setLevel")(level);

        py::list suppress = py::cast(std::vector<std::string>{"naludaq.UART", "naludaq.FTDI"});
        for (auto& name : suppress) {
            logging.attr("getLogger")(name).attr("setLevel")(50);  // CRITICAL
        }
        NaluBoardControllerLogger::debug("Logger setup complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error setting up logger: " + std::string(e.what()));
    }
}

void NaluBoardController::initialize_board() {
    try {
        NaluBoardControllerLogger::debug("Importing 'naludaq.board' module...");
        py::module naludaq_board = py::module::import("naludaq.board");
        NaluBoardControllerLogger::debug("'naludaq.board' module imported.");

        NaluBoardControllerLogger::debug("Creating board object...");
        board = naludaq_board.attr("Board")(model);
        NaluBoardControllerLogger::debug("Board object created.");

        NaluBoardControllerLogger::debug("Getting UDP connection...");
        py::tuple board_ip_tuple = py::make_tuple(board_ip.getIp(), board_ip.getPort());
        py::tuple host_ip_tuple = py::make_tuple(host_ip.getIp(), host_ip.getPort());

        // Call get_udp_connection with tuples
        board.attr("get_udp_connection")(board_ip_tuple, host_ip_tuple);
        NaluBoardControllerLogger::debug("UDP connection established.");

        NaluBoardControllerLogger::debug("Resetting board...");
        py::module naludaq_communication = py::module::import("naludaq.communication");
        board_controller = naludaq_board.attr("get_board_controller")(board);
        board_controller.attr("reset_board")();
        NaluBoardControllerLogger::debug("Board reset.");

        if (!clock_file.empty()) {
            NaluBoardControllerLogger::debug("Loading clock file...");
            board.attr("load_clockfile")(clock_file);
        }
        if (!config_file.empty()) {
            NaluBoardControllerLogger::debug("Loading configuration file...");
            board.attr("load_registers")(config_file);
        }

        NaluBoardControllerLogger::debug("Initializing control registers...");
        control_registers = naludaq_communication.attr("ControlRegisters")(board);
        NaluBoardControllerLogger::debug("Control registers initialized.");

        NaluBoardControllerLogger::debug("Importing 'naludaq.tools.data_collector'...");
        py::module naludaq_tools = py::module::import("naludaq.tools.data_collector");

        NaluBoardControllerLogger::debug("Getting trigger controller...");
        trigger_controller = naludaq_board.attr("get_trigger_controller")(board);

        NaluBoardControllerLogger::debug("Starting up board...");
        py::object startup_board = naludaq_board.attr("startup_board");  
        startup_board(board);

        NaluBoardControllerLogger::debug("Disconnecting board...");
        board.attr("disconnect")();

        NaluBoardControllerLogger::info("Board initialization complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error in initialize_board: " + std::string(e.what()));
    }
}

void NaluBoardController::init_capture(const std::string& target_ip_port, const std::vector<int>& channels, 
                                     int windows, int lookback, int write_after_trig, const std::string& trigger_mode, 
                                     const std::string& lookback_mode, const std::vector<int>& trigger_values, 
                                     const std::vector<int>& dac_values) {
    // Store the parameters into member variables
    // Validate IP addresses
    this->target_ip = IPAddressInfo(target_ip_port);
    this->channels = channels;
    this->readout_window = std::make_tuple(windows, lookback, write_after_trig);
    this->trigger_mode = trigger_mode;
    this->lookback_mode = lookback_mode;
    this->trigger_values = trigger_values;
    this->dac_values = dac_values;
    
    NaluBoardControllerLogger::debug("Capture parameters initialized.");
}

void NaluBoardController::start_capture() {
    // Check if target_ip is set (i.e., not nullptr)
    if (!target_ip.isSet()) {
        NaluBoardControllerLogger::error("Error: target_ip not set. Please call init_capture() first.");
        return;
    }
    try {
        // Construct readout_window tuple
        NaluBoardControllerLogger::debug("Readout window set: windows=" + std::to_string(std::get<0>(readout_window)) +
                                         ", lookback=" + std::to_string(std::get<1>(readout_window)) +
                                         ", write_after_trig=" + std::to_string(std::get<2>(readout_window)));


        // Import the Python module
        NaluBoardControllerLogger::debug("Importing naludaq.controllers...");
        py::module naludaq_conn = py::module::import("naludaq.controllers");

        // Get the connection controller
        NaluBoardControllerLogger::debug("Retrieving connection controller...");
        py::object connection_controller = naludaq_conn.attr("get_connection_controller")(board);

        // Establish UDP connection
        NaluBoardControllerLogger::debug("Establishing UDP connection between board and host...");
        board.attr("get_udp_connection")(py::make_tuple(board_ip.getIp(), board_ip.getPort()), 
                                         py::make_tuple(host_ip.getIp(), host_ip.getPort()));

        // Configure triggers if provided
        if (!trigger_values.empty()) {
            NaluBoardControllerLogger::debug("Setting trigger values...");
            board.attr("trigger_values") = trigger_values;
            py::object trigger_controller = naludaq_conn.attr("get_trigger_controller")(board);
            NaluBoardControllerLogger::debug("Writing trigger values to board...");
            trigger_controller.attr("write_triggers")();
        }

        // Configure DAC values if provided
        if (!dac_values.empty()) {
            NaluBoardControllerLogger::debug("Configuring DAC values...");
            py::object board_controller = naludaq_conn.attr("get_board_controller")(board);
            
            for (size_t chan = 0; chan < dac_values.size(); ++chan) {
                // Check if the channel is included in the channels array
                if (std::find(channels.begin(), channels.end(), chan) != channels.end()) {
                    NaluBoardControllerLogger::debug("Setting DAC value for channel " + std::to_string(chan) + 
                                                     ": " + std::to_string(dac_values[chan]));
                    board_controller.attr("set_single_dac")(chan, dac_values[chan]);
                }
            }
        }

        // Set readout channels
        NaluBoardControllerLogger::debug("Retrieving readout controller...");
        py::object readout_controller = naludaq_conn.attr("get_readout_controller")(board);

        NaluBoardControllerLogger::debug("Checking if readout channels are provided...");
        if (!channels.empty()) {
            NaluBoardControllerLogger::debug("Setting readout channels...");
            // Log the channels being activated
            std::string channel_list = "[";
            for (size_t i = 0; i < channels.size(); ++i) {
                channel_list += std::to_string(channels[i]);
                if (i < channels.size() - 1) {
                    channel_list += ", ";
                }
            }
            channel_list += "]";
            NaluBoardControllerLogger::debug("Activating channels: " + channel_list);

            py::list py_channels;  // Create a Python list
            for (int channel : channels) {
                py_channels.append(channel);  // Append each channel to the Python list
            }
            readout_controller.attr("set_readout_channels")(py_channels);  // Pass the Python list to the function
        } else {
            NaluBoardControllerLogger::debug("No readout channels provided, skipping...");
        }
        
        // Set readout window
        NaluBoardControllerLogger::debug("Configuring readout window...");
        std::apply([&](auto&&... args) {
            readout_controller.attr("set_read_window")(args...);
        }, readout_window);
        
        // Configure the receiver address
        py::object target_ip_tuple = py::make_tuple(target_ip.getIp(), target_ip.getPort());
        NaluBoardControllerLogger::debug("Setting receiver address to " + target_ip.getCombined());
        py::object connection_info = board.attr("connection_info");
        connection_info.attr("__setitem__")("receiver_addr", target_ip_tuple);

        // Configure Ethernet
        NaluBoardControllerLogger::debug("Configuring Ethernet settings...");
        py::object configure_ethernet = connection_controller.attr("_configure_ethernet");
        configure_ethernet();

        // Start readout
        NaluBoardControllerLogger::debug("Retrieving board controller...");
        py::object board_controller = naludaq_conn.attr("get_board_controller")(board);

        NaluBoardControllerLogger::debug("Starting readout with trigger_mode=" + trigger_mode +
                                         " and lookback_mode=" + lookback_mode);
        if (!lookback_mode.empty()) {
            board_controller.attr("start_readout")(py::str(trigger_mode), py::str(lookback_mode));
        } else {
            board_controller.attr("start_readout")(py::str(trigger_mode));
        }

        NaluBoardControllerLogger::info("Data capture started successfully.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error in start_capture: " + std::string(e.what()));
    }
}

void NaluBoardController::start_capture(const std::string& target_ip_port, const std::vector<int>& channels, 
                                     int windows, int lookback, int write_after_trig, const std::string& trigger_mode, 
                                     const std::string& lookback_mode, const std::vector<int>& trigger_values, 
                                     const std::vector<int>& dac_values) {
    init_capture(target_ip_port, channels, windows, lookback, write_after_trig, 
                 trigger_mode, lookback_mode, trigger_values, dac_values);
    start_capture();
}

void NaluBoardController::start_capture(const NaluCaptureParams& params) {
    // Construct the three arrays from NaluCaptureParams:
    
    std::vector<int> channels;
    std::vector<int> trigger_values;
    std::vector<int> dac_values;

    // Loop through the channels map in NaluCaptureParams
    for (const auto& entry : params.channels) {
        if (entry.second.enabled) {
            channels.push_back(entry.first); // Channel ID

            // Trigger values are only used in the immediate ("imm") trigger mode
            if (params.trigger_mode == "imm") {
                trigger_values.push_back(entry.second.trigger_value); // Trigger value for this channel
            }
        }
    
        // Only add to dac_values if assign_dac_values is true
        // There must be a value for each channel regardless of how many are used.
        // Using a map would be a better approach, but the inputs are already confusing enough as they are.
        if (params.assign_dac_values) {
            dac_values.push_back(entry.second.dac_value);  // DAC value for this channel
        }
    }
    
    // Now call the other start_capture method with the constructed arrays
    start_capture(params.target_ip_port, channels, params.windows, params.lookback, 
                  params.write_after_trig, params.trigger_mode, params.lookback_mode, 
                  trigger_values, dac_values);
}


void NaluBoardController::stop_capture() {
    try {
        NaluBoardControllerLogger::debug("Stopping data capture...");
        py::module naludaq_conn = py::module::import("naludaq.controllers");
        py::object board_controller = naludaq_conn.attr("get_board_controller")(board);
        board_controller.attr("stop_readout")();
        board.attr("disconnect")();

        NaluBoardControllerLogger::info("Data capture stopped.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error in stop_capture: " + std::string(e.what()));
    }
}


void NaluBoardController::configure_triggers(const std::vector<int>& trigger_values, bool rising_edge) {
    try {
        if (trigger_controller.is_none()) {
            throw std::runtime_error("Trigger controller not initialized. Call 'initialize_board()' first.");
        }

        NaluBoardControllerLogger::debug("Setting trigger values: " + std::to_string(trigger_values.size()));
        NaluBoardControllerLogger::debug("Rising edge: " + std::to_string(rising_edge));

        py::module naludaq_board = py::module::import("naludaq.board");
        py::module naludaq_communication = py::module::import("naludaq.communication");

        trigger_controller.attr("set_trigger_values")(trigger_values);
        trigger_controller.attr("set_trigger_edge")(py::str("left"), rising_edge);
        trigger_controller.attr("set_trigger_edge")(py::str("right"), rising_edge);

        NaluBoardControllerLogger::info("Trigger configuration complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error configuring triggers: " + std::string(e.what()));
    }
}

void NaluBoardController::enable_ethernet() {
    try {
        if (control_registers.is_none()) {
            throw std::runtime_error("Control registers not initialized. Call 'initialize_board()' first.");
        }

        control_registers.attr("write")(py::str("iomode0"), true);  // Disable serial
        control_registers.attr("write")(py::str("iomode1"), false); // Enable Ethernet

        NaluBoardControllerLogger::info("Ethernet communication enabled.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error enabling Ethernet: " + std::string(e.what()));
    }
}

void NaluBoardController::enable_serial() {
    try {
        if (control_registers.is_none()) {
            throw std::runtime_error("Control registers not initialized. Call 'initialize_board()' first.");
        }

        control_registers.attr("write")(py::str("iomode0"), false);  // Enable serial
        control_registers.attr("write")(py::str("iomode1"), true);   // Disable Ethernet

        NaluBoardControllerLogger::info("Serial communication enabled.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error("Error enabling serial communication: " + std::string(e.what()));
    }
}
