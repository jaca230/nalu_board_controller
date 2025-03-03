#ifndef NALU_BOARD_CONTROLLER_PARAMS_H
#define NALU_BOARD_CONTROLLER_PARAMS_H

#include <string>
#include <vector>
#include <map>

// Define NaluChannelInfo with default values
struct NaluChannelInfo {
    bool enabled = true;
    int trigger_value = 0;
    int dac_value = 0;
};

// NaluBoardParams definition
struct NaluBoardParams {
    std::string model = "HDSOCv1_evalr2";
    std::string board_ip_port = "192.168.1.59:4660";
    std::string host_ip_port = "192.168.1.1:4660";
    std::string config_file = "";
    std::string clock_file = "";
};

// NaluCaptureParams definition with map for channels
struct NaluCaptureParams {
    std::string target_ip_port = "192.168.1.1:12345";
    int windows = 1;
    int lookback = 1;
    int write_after_trig = 1;
    bool assign_dac_values = false;
    std::string trigger_mode = "ext";
    std::string lookback_mode = "";

    // Map to store NaluChannelInfo for each channel
    std::map<int, NaluChannelInfo> channels;
};

// NaluCaptureParamsWrapper that initializes the map
class NaluCaptureParamsWrapper {
public:
    // Constructor that initializes the channel map
    NaluCaptureParamsWrapper(int num_channels = 32) {
        // Loop through and create a default NaluChannelInfo for each channel
        for (int i = 0; i < num_channels; ++i) {
            capture_params.channels[i] = NaluChannelInfo();  // Default NaluChannelInfo
        }
    }

    // Method to retrieve the NaluCaptureParams struct
    NaluCaptureParams get_capture_params() {
        return capture_params;
    }

private:
    NaluCaptureParams capture_params;
};

#endif // NALU_BOARD_CONTROLLER_PARAMS_H
