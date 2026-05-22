#include "nalu_board_controller/runtime/configurator.h"

#include <algorithm>
#include <sstream>

#include <spdlog/spdlog.h>

#include "nalu_board_controller/python/backend.h"

namespace {

std::string join_ints(const std::vector<int>& values) {
    std::ostringstream stream;
    stream << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            stream << ", ";
        }
        stream << values[i];
    }
    stream << "]";
    return stream.str();
}

bool same_wlc_mode(const nalu_board_controller::WindowLevelControlConfig& lhs,
                   const nalu_board_controller::WindowLevelControlConfig& rhs) {
    return lhs.configure == rhs.configure && lhs.enabled == rhs.enabled;
}

py::dict build_reference_groups(const nalu_board_controller::State& state) {
    py::dict references;
    const auto& channels = state.channel_settings();
    if (channels.empty()) {
        return references;
    }

    const int max_channel = channels.rbegin()->first;
    for (int start = 0; start <= max_channel; start += 16) {
        const int end = std::min(start + 15, max_channel);
        references[py::str(std::to_string(start) + "_" + std::to_string(end))] =
            py::make_tuple(state.trigger().low_reference, state.trigger().high_reference);
    }
    return references;
}

}  // namespace

namespace nalu_board_controller {

Configurator::Configurator(State* state, PythonBackend* python_backend)
    : state_(state), python_backend_(python_backend) {}

void Configurator::configure_for_capture() {
    spdlog::debug("Starting capture configuration");
    configure_board_mode();
    configure_triggers();
    configure_dac_values();
    configure_readout();
    configure_connection();
    spdlog::debug("Capture configuration completed");
}

void Configurator::configure_board_mode() {
    const auto& window_level_control = state_->window_level_control();
    if (!state_->has_initialized_window_level_control()) {
        if (!window_level_control.configure) {
            spdlog::debug("WLC configuration disabled; leaving board mode unchanged");
            return;
        }
        spdlog::warn(
            "WLC mode is configured for this run, but no board-initialization WLC state was recorded. "
            "WLC changes are only applied during board initialization; continuing without changing board mode.");
        return;
    }

    if (!same_wlc_mode(window_level_control, state_->initialized_window_level_control())) {
        spdlog::warn(
            "Requested WLC mode change at run start ignored. WLC changes are only applied during board initialization. "
            "initialized=(configure={}, enabled={}), requested=(configure={}, enabled={})",
            state_->initialized_window_level_control().configure,
            state_->initialized_window_level_control().enabled,
            window_level_control.configure,
            window_level_control.enabled);
        return;
    }

    if (!window_level_control.configure) {
        spdlog::debug("WLC configuration disabled and matches board initialization; leaving board mode unchanged");
        return;
    }

    spdlog::debug(
        "WLC mode already matches board initialization; no run-start board-mode changes applied");
}

void Configurator::configure_triggers() {
    if (state_->trigger().mode != "self") {
        spdlog::debug(
            "Skipping trigger-controller programming for readout mode '{}'",
            state_->trigger().mode);
        return;
    }

    if (state_->trigger_values().empty()) {
        spdlog::debug("No trigger values available; skipping trigger configuration");
        return;
    }

    try {
        auto& trigger_controller = python_backend_->trigger_controller();

        py::list py_trigger_values;
        for (int value : state_->trigger_values()) {
            py_trigger_values.append(value);
        }
        trigger_controller.attr("values") = py_trigger_values;
        trigger_controller.attr("write_triggers")();

        py::dict references = build_reference_groups(*state_);
        if (py::len(references) > 0) {
            trigger_controller.attr("references") = references;
        }

        trigger_controller.attr("set_trigger_edge")(py::str("left"), state_->trigger().rising_edge);
        trigger_controller.attr("set_trigger_edge")(py::str("right"), state_->trigger().rising_edge);

        spdlog::debug("Configured trigger values {} with references {}-{} for readout mode '{}'",
                      join_ints(state_->trigger_values()),
                      state_->trigger().low_reference,
                      state_->trigger().high_reference,
                      state_->trigger().mode);
    } catch (const py::error_already_set& error) {
        spdlog::error("Trigger configuration failed: {}", error.what());
        throw;
    }
}

void Configurator::configure_dac_values() {
    if (!state_->assign_dac_values()) {
        spdlog::debug("DAC assignment disabled; skipping DAC configuration");
        return;
    }

    const auto& channel_settings = state_->channel_settings();
    if (channel_settings.empty()) {
        spdlog::debug("No channel settings available for DAC configuration");
        return;
    }

    try {
        for (int channel : state_->enabled_channels()) {
            const auto it = channel_settings.find(channel);
            if (it == channel_settings.end()) {
                continue;
            }
            python_backend_->dac_controller().attr("set_single_dac")(channel, it->second.dac_value);
            spdlog::debug("Set DAC for channel {} to {}", channel, it->second.dac_value);
        }
    } catch (const py::error_already_set& error) {
        spdlog::error("DAC configuration failed: {}", error.what());
        throw;
    }
}

void Configurator::configure_readout() {
    try {
        auto& readout_controller = python_backend_->readout_controller();
        const auto& readout_window = state_->readout_window();

        py::list py_channels;
        for (int channel : state_->enabled_channels()) {
            py_channels.append(channel);
        }

        readout_controller.attr("set_readout_channels")(py_channels);
        readout_controller.attr("set_read_window")(
            readout_window.windows,
            readout_window.lookback,
            readout_window.write_after_trigger);

        spdlog::debug("Configured readout channels {} and window ({}, {}, {})",
                      join_ints(state_->enabled_channels()),
                      readout_window.windows,
                      readout_window.lookback,
                      readout_window.write_after_trigger);
    } catch (const py::error_already_set& error) {
        spdlog::error("Readout controller configuration failed: {}", error.what());
        throw;
    }
}

void Configurator::configure_connection() {
    try {
        python_backend_->connection_controller().attr("_configure_ethernet")();
        spdlog::debug("Ethernet connection configured");
    } catch (const py::error_already_set& error) {
        spdlog::error("Connection configuration failed: {}", error.what());
        throw;
    }
}

}  // namespace nalu_board_controller
