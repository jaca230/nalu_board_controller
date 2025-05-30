#include "nalu_board_state.h"
#include <algorithm> // for std::transform

NaluBoardState::NaluBoardState(const NaluBoardParams& params) 
    : model_(params.model),  // Initialize first
      board_ip_(params.board_ip_port),
      host_ip_(params.host_ip_port),
      config_file_(params.config_file),
      clock_file_(params.clock_file) {
    
    // Convert model_ to lowercase in-place
    std::transform(model_.begin(), model_.end(), model_.begin(), ::tolower);
}

void NaluBoardState::UpdateFromCaptureParams(const NaluCaptureParams& params) {
    target_ip_ = IPAddressInfo(params.target_ip_port);
    channels_.clear();
    trigger_values_.clear();
    dac_values_.clear();

    for (const auto& [channel_num, channel_info] : params.channels) {
        trigger_values_.push_back(channel_info.trigger_value);
        dac_values_.push_back(channel_info.dac_value);

        if (channel_info.enabled) {
            channels_.push_back(channel_num);
        }
    }

    readout_window_ = std::make_tuple(params.windows, params.lookback, params.write_after_trig);
    trigger_mode_ = params.trigger_mode;
    lookback_mode_ = params.lookback_mode;
    low_reference_ = params.low_reference;
    high_reference_ = params.high_reference;
    rising_edge_ = params.rising_edge;
    assign_dac_values_ = params.assign_dac_values;
}