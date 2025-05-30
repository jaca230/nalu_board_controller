#ifndef NALU_BOARD_STATE_H
#define NALU_BOARD_STATE_H

#include <string>
#include <vector>
#include <tuple>
#include "ip_address_info.h"
#include "nalu_board_controller_params.h"

class NaluBoardState {
public:
    explicit NaluBoardState(const NaluBoardParams& params);
    
    // Board initialization
    bool IsInitialized() const { return is_initialized_; }
    void SetInitialized(bool initialized) { is_initialized_ = initialized; }

    // Board configuration
    const std::string& Model() const { return model_; }
    const IPAddressInfo& BoardIp() const { return board_ip_; }
    const IPAddressInfo& HostIp() const { return host_ip_; }
    const std::string& ConfigFile() const { return config_file_; }
    const std::string& ClockFile() const { return clock_file_; }

    // Capture configuration
    const IPAddressInfo& TargetIp() const { return target_ip_; }
    const std::tuple<int, int, int>& ReadoutWindow() const { return readout_window_; }
    const std::string& TriggerMode() const { return trigger_mode_; }
    const std::string& LookbackMode() const { return lookback_mode_; }
    const std::vector<int>& Channels() const { return channels_; }
    const std::vector<int>& TriggerValues() const { return trigger_values_; }
    const std::vector<int>& DacValues() const { return dac_values_; }
    int HighReference() const { return high_reference_; }
    int LowReference() const { return low_reference_; }
    bool RisingEdge() const { return rising_edge_; }
    bool AssignDacValues() const { return assign_dac_values_; }

    // Setters for capture configuration (only provide setters for what should be mutable)
    void SetTargetIp(const IPAddressInfo& ip) { target_ip_ = ip; }
    void SetReadoutWindow(int windows, int lookback, int write_after_trig) {
        readout_window_ = std::make_tuple(windows, lookback, write_after_trig);
    }
    void SetTriggerMode(const std::string& mode) { trigger_mode_ = mode; }
    void SetLookbackMode(const std::string& mode) { lookback_mode_ = mode; }
    void SetHighReference(int value) { high_reference_ = value; }
    void SetLowReference(int value) { low_reference_ = value; }
    void SetRisingEdge(bool rising_edge) { rising_edge_ = rising_edge; }
    void SetAssignDacValues(bool assign) { assign_dac_values_ = assign; }

    // Bulk update from capture params
    void UpdateFromCaptureParams(const NaluCaptureParams& params);

private:
    bool is_initialized_ = false;
    std::string model_;
    IPAddressInfo board_ip_;
    IPAddressInfo host_ip_;
    std::string config_file_;
    std::string clock_file_;

    // Capture-related state
    IPAddressInfo target_ip_;
    std::tuple<int, int, int> readout_window_;
    std::string trigger_mode_;
    std::string lookback_mode_;
    std::vector<int> channels_;
    std::vector<int> trigger_values_;
    std::vector<int> dac_values_;
    int high_reference_;
    int low_reference_;
    bool rising_edge_;
    bool assign_dac_values_;
};

#endif // NALU_BOARD_STATE_H