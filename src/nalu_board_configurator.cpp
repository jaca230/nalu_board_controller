#include "nalu_board_configurator.h"
#include "nalu_board_controller_logger.h"
#include <algorithm>  // for std::transform


NaluBoardConfigurator::NaluBoardConfigurator(NaluBoardState* state, 
                                           NaluBoardPythonWrapper* python_wrapper)
    : state_(state), python_wrapper_(python_wrapper) {}

void NaluBoardConfigurator::ConfigureForCapture() {
    NaluBoardControllerLogger::debug("Starting full capture configuration...");
    ConfigureTriggers();
    ConfigureDacValues();
    ConfigureReadoutController();
    ConfigureConnection();
    NaluBoardControllerLogger::debug("Capture configuration completed.");
}

void NaluBoardConfigurator::ConfigureTriggers() {
    std::string trigger_mode = state_->TriggerMode();
    std::transform(trigger_mode.begin(), trigger_mode.end(), trigger_mode.begin(), ::tolower);

    if (trigger_mode != "self") {
        NaluBoardControllerLogger::debug("Trigger mode is '" + trigger_mode + "', not 'self', skipping trigger configuration.");
        return;
    }

    if (state_->TriggerValues().empty()) {
        NaluBoardControllerLogger::debug("No trigger values to configure");
        return;
    }

    try {
        NaluBoardControllerLogger::debug("Configuring triggers...");

        py::object trigger_controller = python_wrapper_->TriggerController();

        // Log current trigger values
        std::string trigger_values_str = "[";
        for (size_t i = 0; i < state_->TriggerValues().size(); ++i) {
            trigger_values_str += std::to_string(state_->TriggerValues()[i]);
            if (i < state_->TriggerValues().size() - 1) trigger_values_str += ", ";
        }
        trigger_values_str += "]";
        NaluBoardControllerLogger::debug("Trigger values to set: " + trigger_values_str);

        // Set trigger values
        py::list py_trigger_values;
        for (int val : state_->TriggerValues()) {
            py_trigger_values.append(val);
        }
        trigger_controller.attr("values") = py_trigger_values;
        NaluBoardControllerLogger::debug("Trigger values assigned to Python controller.");

        trigger_controller.attr("write_triggers")();
        NaluBoardControllerLogger::debug("write_triggers() called on trigger controller.");

        // Set reference values
        py::dict references;
        references["left"] = py::make_tuple(state_->LowReference(), state_->HighReference());
        references["right"] = py::make_tuple(state_->LowReference(), state_->HighReference());
        trigger_controller.attr("references") = references;

        NaluBoardControllerLogger::debug(
            "Trigger references set to: left = (" + 
            std::to_string(state_->LowReference()) + ", " + 
            std::to_string(state_->HighReference()) + "), right = (" +
            std::to_string(state_->LowReference()) + ", " + 
            std::to_string(state_->HighReference()) + ")"
        );

        // Set trigger edges
        // True --> Rising Edge
        // False --> Falling Edge
        trigger_controller.attr("set_trigger_edge")(py::str("left"), state_->RisingEdge());
        trigger_controller.attr("set_trigger_edge")(py::str("right"), state_->RisingEdge());

        NaluBoardControllerLogger::debug(
            "Trigger edges set to: left = " + std::string(state_->RisingEdge() ? "Rising" : "Falling") +
            ", right = " + std::string(state_->RisingEdge() ? "Rising" : "Falling")
        );

        NaluBoardControllerLogger::debug("Trigger configuration complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Trigger configuration error: ") + e.what());
        throw;
    }
}

void NaluBoardConfigurator::ConfigureDacValues() {
    if (!state_->AssignDacValues()) {
        NaluBoardControllerLogger::debug("DAC values assignment is disabled, skipping configuration.");
        return;
    }
    if (state_->DacValues().empty()) {
        NaluBoardControllerLogger::debug("No DAC values to configure");
        return;
    }

    try {
        NaluBoardControllerLogger::debug("Configuring DAC values...");

        std::string dac_values_str = "DAC values for channels [";
        bool first = true;

        for (size_t chan = 0; chan < state_->DacValues().size(); ++chan) {
            if (std::find(state_->Channels().begin(), state_->Channels().end(), chan) != state_->Channels().end()) {
                if (!first) {
                    dac_values_str += ", ";
                }
                dac_values_str += "ch" + std::to_string(chan) + "=" + std::to_string(state_->DacValues()[chan]);
                first = false;
            }
        }
        dac_values_str += "]";
        NaluBoardControllerLogger::debug(dac_values_str);

        for (size_t chan = 0; chan < state_->DacValues().size(); ++chan) {
            if (std::find(state_->Channels().begin(), state_->Channels().end(), chan) != state_->Channels().end()) {
                NaluBoardControllerLogger::debug(
                    "Setting DAC for channel " + std::to_string(chan) + " to " + std::to_string(state_->DacValues()[chan])
                );
                python_wrapper_->DacController().attr("set_single_dac")(chan, state_->DacValues()[chan]);
            }
        }
        NaluBoardControllerLogger::debug("DAC configuration complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("DAC configuration error: ") + e.what());
        throw;
    }
}

void NaluBoardConfigurator::ConfigureReadoutController() {
    try {
        NaluBoardControllerLogger::debug("Configuring readout controller...");

        py::object readout_controller = python_wrapper_->ReadoutController();
        auto [windows, lookback, write_after_trig] = state_->ReadoutWindow();

        NaluBoardControllerLogger::debug(
            "Readout window params - windows: " + std::to_string(windows) +
            ", lookback: " + std::to_string(lookback) +
            ", write_after_trig: " + std::to_string(write_after_trig)
        );

        // Set readout channels
        if (!state_->Channels().empty()) {
            py::list py_channels;
            std::string channels_str = "Readout channels: [";
            bool first = true;
            for (int channel : state_->Channels()) {
                py_channels.append(channel);
                if (!first) {
                    channels_str += ", ";
                }
                channels_str += std::to_string(channel);
                first = false;
            }
            channels_str += "]";
            NaluBoardControllerLogger::debug(channels_str);

            readout_controller.attr("set_readout_channels")(py_channels);
            NaluBoardControllerLogger::debug("set_readout_channels() called.");
        } else {
            NaluBoardControllerLogger::debug("No readout channels set.");
        }

        readout_controller.attr("set_read_window")(windows, lookback, write_after_trig);
        NaluBoardControllerLogger::debug("set_read_window() called with parameters.");

        NaluBoardControllerLogger::debug("Readout controller configuration complete.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Readout controller configuration error: ") + e.what());
        throw;
    }
}

void NaluBoardConfigurator::ConfigureConnection() {
    try {
        NaluBoardControllerLogger::debug("Configuring connection controller...");

        py::object connection_controller = python_wrapper_->ConnectionController();
        connection_controller.attr("_configure_ethernet")();

        NaluBoardControllerLogger::debug("Connection controller configured successfully.");
    } catch (const py::error_already_set& e) {
        NaluBoardControllerLogger::error(std::string("Connection configuration error: ") + e.what());
        throw;
    }
}
