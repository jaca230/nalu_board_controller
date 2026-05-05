#include "nalu_board_controller/controller/controller.h"

#include <algorithm>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/channel_config.h"
#include "nalu_board_controller/config/readout_window_config.h"
#include "nalu_board_controller/config/trigger_config.h"
#include "nalu_board_controller/logging/logging.h"
#include "nalu_board_controller/python/backend.h"
#include "nalu_board_controller/runtime/configurator.h"
#include "nalu_board_controller/runtime/state.h"

namespace {

int infer_channel_count(const std::vector<int>& channels,
                        const std::vector<int>& trigger_values,
                        const std::vector<int>& dac_values) {
    int count = 0;
    if (!channels.empty()) {
        count = *std::max_element(channels.begin(), channels.end()) + 1;
    }
    count = std::max(count, static_cast<int>(trigger_values.size()));
    count = std::max(count, static_cast<int>(dac_values.size()));
    return std::max(count, 0);
}

}  // namespace

namespace nalu_board_controller {

Controller::Controller(const BoardConfig& config) {
    logging::initialize();

    state_ = std::make_unique<State>(config);
    python_backend_ = std::make_unique<PythonBackend>(state_.get());
    configurator_ = std::make_unique<Configurator>(state_.get(), python_backend_.get());
}

Controller::~Controller() = default;

void Controller::sync_python_logging(int level) {
    python_backend_->sync_python_logging(level);
}

void Controller::sync_python_logging(const std::string& level) {
    python_backend_->sync_python_logging(level);
}

void Controller::initialize_board() {
    python_backend_->initialize_board();
    state_->set_initialized(true);
}

void Controller::start_capture(const CaptureConfig& config) {
    init_capture(config);
    python_backend_->start_capture();
}

void Controller::start_capture(const std::string& target_endpoint,
                               const std::vector<int>& channels,
                               const ReadoutWindowConfig& readout_window,
                               const TriggerConfig& trigger,
                               const std::vector<int>& trigger_values,
                               const std::vector<int>& dac_values,
                               const WindowLevelControlConfig& window_level_control) {
    CaptureConfig config;
    config.target_endpoint = target_endpoint;
    config.readout_window = readout_window;
    config.trigger = trigger;
    config.window_level_control = window_level_control;
    config.assign_dac_values = !dac_values.empty();

    const int channel_count = infer_channel_count(channels, trigger_values, dac_values);
    for (int channel = 0; channel < channel_count; ++channel) {
        ChannelConfig channel_config;
        channel_config.enabled = std::find(channels.begin(), channels.end(), channel) != channels.end();
        if (channel < static_cast<int>(trigger_values.size())) {
            channel_config.trigger_value = trigger_values[channel];
        }
        if (channel < static_cast<int>(dac_values.size())) {
            channel_config.dac_value = dac_values[channel];
        }
        config.channels[channel] = channel_config;
    }

    init_capture(config);
    python_backend_->start_capture();
}

void Controller::stop_capture() {
    python_backend_->stop_capture();
}

void Controller::enable_ethernet() {
    python_backend_->enable_ethernet();
}

void Controller::enable_serial() {
    python_backend_->enable_serial();
}

void Controller::init_capture(const CaptureConfig& config) {
    if (!state_->is_initialized()) {
        spdlog::error("Board not initialized. Call initialize_board() first.");
        throw std::runtime_error("Board not initialized");
    }

    state_->update_from_capture_config(config);
    configurator_->configure_for_capture();
}

}  // namespace nalu_board_controller
