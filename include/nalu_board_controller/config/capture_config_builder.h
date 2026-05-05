#ifndef NALU_BOARD_CONTROLLER_CONFIG_CAPTURE_CONFIG_BUILDER_H
#define NALU_BOARD_CONTROLLER_CONFIG_CAPTURE_CONFIG_BUILDER_H

#include "nalu_board_controller/config/capture_config.h"

namespace nalu_board_controller {

class CaptureConfigBuilder {
public:
    explicit CaptureConfigBuilder(int channel_count = 32) {
        for (int channel = 0; channel < channel_count; ++channel) {
            config_.channels[channel] = ChannelConfig();
        }
    }

    CaptureConfig build() const { return config_; }

private:
    CaptureConfig config_;
};

}  // namespace nalu_board_controller

#endif
