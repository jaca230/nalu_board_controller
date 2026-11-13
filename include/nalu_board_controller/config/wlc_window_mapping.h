#ifndef NALU_BOARD_CONTROLLER_CONFIG_WLC_WINDOW_MAPPING_H
#define NALU_BOARD_CONTROLLER_CONFIG_WLC_WINDOW_MAPPING_H

#include <stdexcept>
#include <string>

namespace nalu_board_controller {

constexpr int wlc_min_asic_windows = 1;
constexpr int wlc_max_asic_windows = 12;

constexpr int wlc_returned_windows(int asic_windows) {
    return 2 * ((asic_windows + 1) / 2) + 2;
}

constexpr bool is_valid_wlc_returned_windows(int returned_windows) {
    return returned_windows >= wlc_returned_windows(wlc_min_asic_windows) &&
           returned_windows <= wlc_returned_windows(wlc_max_asic_windows) &&
           returned_windows % 2 == 0;
}

inline int wlc_asic_windows_for_returned(int returned_windows) {
    if (!is_valid_wlc_returned_windows(returned_windows)) {
        throw std::invalid_argument(
            "WLC cannot return exactly " + std::to_string(returned_windows) +
            " windows. WLC returns R = 2 * ceil(N / 2) + 2 windows for an ASIC "
            "request N in [1, 12], so the supported returned window counts are "
            "the even values 4, 6, 8, 10, 12, and 14.");
    }

    // Either member of an adjacent request pair produces the same result. Use
    // the even member so the inverse is simply R - 2.
    return returned_windows - 2;
}

}  // namespace nalu_board_controller

#endif
