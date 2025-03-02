
#ifndef NALU_CAPTURE_PARAMS_H
#define NALU_CAPTURE_PARAMS_H

#include <string>
#include <vector>

struct NaluCaptureParams {
    std::string target_ip_port = "192.168.1.1:12345";
    std::vector<int> channels = { 0, 1, 2, 3, 4, 5, 6, 7,
                                  8, 9, 10, 11, 12, 13, 14, 15,
                                  16, 17, 18, 19, 20, 21, 22, 23,
                                  24, 25, 26, 27, 28, 29, 30, 31 };
    int windows = 4;
    int lookback = 4;
    int write_after_trig = 4;
    std::string trigger_mode = "ext";
    std::string lookback_mode = "";
    std::vector<int> trigger_values = {};
    std::vector<int> dac_values = {};
};

#endif // NALU_CAPTURE_PARAMS_H
