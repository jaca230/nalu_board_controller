#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "nalu_board_controller/config/board_config.h"
#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/wlc_window_mapping.h"
#include "nalu_board_controller/controller/controller.h"

namespace firmware_959_scan {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

struct Stats {
    uint64_t datagrams = 0;
    uint64_t payload_bytes = 0;
    uint64_t packets = 0;
    uint64_t malformed_bytes = 0;
    uint64_t trigger_groups = 0;
    uint64_t raw_file_bytes = 0;
    bool raw_event_limit_reached = false;
    std::map<int, uint64_t> channel_packets;
    std::map<int, uint64_t> channel_trigger_groups;
    std::map<int, uint64_t> logical_positions;
    std::map<int, uint32_t> last_trigger_time;
    Clock::time_point first_packet{};
    Clock::time_point last_packet{};
    bool has_packets = false;
    int anchor_channel = -1;
};

inline json read_json(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open config: " + path);
    }
    json value;
    input >> value;
    return value;
}

class UdpRecorder {
  public:
    UdpRecorder(std::string address,
                uint16_t port,
                size_t header_bytes,
                size_t receive_buffer_bytes,
                std::string raw_output_path = {},
                uint64_t max_raw_events = 0)
        : address_(std::move(address)),
          port_(port),
          header_bytes_(header_bytes),
          receive_buffer_bytes_(receive_buffer_bytes),
          raw_output_path_(std::move(raw_output_path)),
          max_raw_events_(max_raw_events) {}

    void start() {
        if (!raw_output_path_.empty()) {
            const std::filesystem::path raw_path(raw_output_path_);
            if (raw_path.has_parent_path()) {
                std::filesystem::create_directories(raw_path.parent_path());
            }
            raw_output_.open(raw_output_path_, std::ios::binary | std::ios::trunc);
            if (!raw_output_) {
                throw std::runtime_error("Cannot open raw output: " + raw_output_path_);
            }
            static constexpr char magic[] = {'N', 'A', 'L', 'U', 'U', 'D', 'P', '1'};
            raw_output_.write(magic, sizeof(magic));
            stats_.raw_file_bytes += sizeof(magic);
        }

        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) {
            throw std::runtime_error("socket() failed");
        }

        int reuse_address = 1;
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));
        if (receive_buffer_bytes_ > 0) {
            const int requested = static_cast<int>(receive_buffer_bytes_);
            setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &requested, sizeof(requested));
        }
        timeval timeout{0, 200000};
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(port_);
        if (inet_pton(AF_INET, address_.c_str(), &endpoint.sin_addr) != 1 ||
            bind(socket_, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) < 0) {
            ::close(socket_);
            socket_ = -1;
            throw std::runtime_error("Unable to bind UDP receiver " + address_ + ":" +
                                     std::to_string(port_));
        }

        started_at_ = Clock::now();
        running_ = true;
        worker_ = std::thread([this] { receive(); });
    }

    void begin_measurement() {
        measurement_enabled_ = true;
    }

    void end_measurement() {
        measurement_enabled_ = false;
    }

    Stats stop() {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
        if (socket_ >= 0) {
            ::close(socket_);
            socket_ = -1;
        }
        if (raw_output_.is_open()) {
            raw_output_.flush();
            raw_output_.close();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    uint64_t elapsed_ns(Clock::time_point time) const {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(time - started_at_).count());
    }

    ~UdpRecorder() {
        if (running_) {
            stop();
        }
    }

  private:
    static void write_u32_le(std::ostream& output, uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            output.put(static_cast<char>((value >> shift) & 0xff));
        }
    }

    static void write_u64_le(std::ostream& output, uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            output.put(static_cast<char>((value >> shift) & 0xff));
        }
    }

    void record_raw_datagram(const uint8_t* data, size_t size, Clock::time_point received_at) {
        if (!raw_output_.is_open()) {
            return;
        }
        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    received_at - started_at_)
                                    .count();
        write_u64_le(raw_output_, static_cast<uint64_t>(elapsed_ns));
        write_u32_le(raw_output_, static_cast<uint32_t>(size));
        raw_output_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        stats_.raw_file_bytes += 12 + size;
    }

    void record_packet(const std::vector<uint8_t>& stream, size_t offset, Clock::time_point now) {
        const int channel = stream[offset + 1] & 0x3f;
        const uint16_t trigger_upper =
            static_cast<uint16_t>((stream[offset + 2] << 8) | stream[offset + 3]);
        const uint16_t trigger_lower =
            static_cast<uint16_t>((stream[offset + 4] << 8) | stream[offset + 5]);
        const uint32_t trigger_time =
            (static_cast<uint32_t>(trigger_upper) << 12) | (trigger_lower & 0x0fff);
        const int logical = ((stream[offset + 6] & 0x3f) << 2) |
                            ((stream[offset + 7] >> 6) & 0x03);

        std::lock_guard<std::mutex> lock(mutex_);
        if (!stats_.has_packets) {
            stats_.first_packet = now;
        }
        stats_.last_packet = now;
        stats_.has_packets = true;
        ++stats_.packets;
        ++stats_.channel_packets[channel];
        ++stats_.logical_positions[logical];

        const auto prior = stats_.last_trigger_time.find(channel);
        if (prior == stats_.last_trigger_time.end() || prior->second != trigger_time) {
            stats_.last_trigger_time[channel] = trigger_time;
            ++stats_.channel_trigger_groups[channel];
            if (stats_.anchor_channel < 0) {
                stats_.anchor_channel = channel;
            }
            if (channel == stats_.anchor_channel) {
                ++stats_.trigger_groups;
            }
        }
    }

    void receive() {
        std::vector<uint8_t> datagram(65536);
        std::vector<uint8_t> stream;
        while (running_) {
            const ssize_t received = recv(socket_, datagram.data(), datagram.size(), 0);
            if (received <= 0) {
                continue;
            }
            const auto now = Clock::now();
            const size_t datagram_size = static_cast<size_t>(received);
            const size_t payload_start = std::min(header_bytes_, datagram_size);
            if (!measurement_enabled_) {
                stream.clear();
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++stats_.datagrams;
                stats_.payload_bytes += datagram_size - payload_start;
            }

            stream.insert(stream.end(), datagram.begin() + payload_start,
                          datagram.begin() + received);
            size_t offset = 0;
            while (offset + 74 <= stream.size()) {
                if (stream[offset] != 0x0e || stream[offset + 72] != 0xfa ||
                    stream[offset + 73] != 0x5a) {
                    ++offset;
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++stats_.malformed_bytes;
                    continue;
                }
                record_packet(stream, offset, now);
                offset += 74;
            }
            if (offset > 0) {
                stream.erase(stream.begin(), stream.begin() + static_cast<long>(offset));
            }

            std::lock_guard<std::mutex> lock(mutex_);
            if (!stats_.raw_event_limit_reached &&
                (max_raw_events_ == 0 || stats_.trigger_groups <= max_raw_events_)) {
                record_raw_datagram(datagram.data(), datagram_size, now);
            } else if (max_raw_events_ > 0 && stats_.trigger_groups > max_raw_events_) {
                stats_.raw_event_limit_reached = true;
            }
        }
    }

    std::string address_;
    uint16_t port_;
    size_t header_bytes_;
    size_t receive_buffer_bytes_;
    std::string raw_output_path_;
    uint64_t max_raw_events_ = 0;
    int socket_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> measurement_enabled_{false};
    std::thread worker_;
    std::mutex mutex_;
    Stats stats_;
    Clock::time_point started_at_{};
    std::ofstream raw_output_;
};

inline nalu_board_controller::BoardConfig board_config(const json& value) {
    nalu_board_controller::BoardConfig output;
    output.model = value.value("model", output.model);
    output.board_endpoint = value.value("board_endpoint", output.board_endpoint);
    output.host_endpoint = value.value("host_endpoint", output.host_endpoint);
    output.asic_serial_mode = value.value("asic_serial_mode", output.asic_serial_mode);
    output.config_file = value.value("config_file", output.config_file);
    output.clock_file = value.value("clock_file", output.clock_file);
    return output;
}

inline nalu_board_controller::CaptureConfig capture_config(
    const json& root,
    bool wlc,
    int requested_windows,
    const std::vector<int>& active_channels) {
    const auto& value = root.at("capture");
    nalu_board_controller::CaptureConfig output;
    output.target_endpoint = value.value("target_endpoint", output.target_endpoint);
    output.assign_dac_values = value.value("assign_dac_values", false);
    output.readoutchannels_register = value.value("readoutchannels_register", -1);
    output.readout_window.windows = requested_windows;
    output.readout_window.lookback = value.value("lookback", requested_windows);
    output.readout_window.write_after_trigger = value.value("write_after_trigger", requested_windows);
    output.readout_window.lookback_mode = value.value("lookback_mode", "trigrel");
    output.trigger.mode = value.value("trigger_mode", "self");
    output.trigger.low_reference = value.value("low_reference", 5);
    output.trigger.high_reference = value.value("high_reference", 11);
    output.trigger.rising_edge = value.value("rising_edge", true);
    output.window_level_control.enabled = wlc;
    output.window_level_control.reinitialize_after_change = true;

    const int channel_count = value.value("channel_count", 32);
    const int trigger_value = value.value("trigger_value", 1);
    const int dac_value = value.value("dac_value", 1804);
    for (int channel : active_channels) {
        if (channel < 0 || channel >= channel_count) {
            throw std::runtime_error("Invalid active channel: " + std::to_string(channel));
        }
    }
    for (int channel = 0; channel < channel_count; ++channel) {
        const bool enabled = std::find(active_channels.begin(), active_channels.end(), channel) !=
                             active_channels.end();
        output.channels[channel] = {enabled, trigger_value, dac_value};
    }
    return output;
}

template <typename Value>
inline json channel_map_json(const std::map<int, Value>& values) {
    json output = json::object();
    for (const auto& [channel, value] : values) {
        output[std::to_string(channel)] = value;
    }
    return output;
}

enum class CaptureAction {
    ConfigureAndStart,
    AlreadyRunning,
};

inline json measure(nalu_board_controller::Controller& controller,
                    const json& document,
                    bool wlc,
                    int windows,
                    const std::vector<int>& channels,
                    double duration_seconds,
                    CaptureAction capture_action,
                    bool stop_capture_after_measurement,
                    double capture_restart_warmup_seconds,
                    const std::string& raw_output_path,
                    const std::atomic<bool>& interrupted) {
    const auto& receiver = document.at("receiver");
    const auto raw_capture = document.value("raw_capture", json::object());
    const uint64_t max_raw_events = raw_capture.value("max_events", uint64_t{0});
    UdpRecorder recorder(receiver.value("address", "192.168.1.1"),
                         receiver.value("port", 12345),
                         receiver.value("udp_header_bytes", 16),
                         receiver.value("receive_buffer_bytes", 16 * 1024 * 1024),
                         raw_output_path,
                         max_raw_events);
    recorder.start();
    bool capture_running = capture_action == CaptureAction::AlreadyRunning;
    const auto capture = capture_config(document, wlc, windows, channels);
    Clock::time_point measurement_start;
    Clock::time_point measurement_end;
    try {
        if (capture_action == CaptureAction::ConfigureAndStart) {
            controller.start_capture(capture);
            capture_running = true;
            const auto warmup_deadline =
                Clock::now() + std::chrono::duration<double>(capture_restart_warmup_seconds);
            while (!interrupted && Clock::now() < warmup_deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        measurement_start = Clock::now();
        recorder.begin_measurement();
        const auto deadline = measurement_start + std::chrono::duration<double>(duration_seconds);
        while (!interrupted && Clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        measurement_end = Clock::now();
        recorder.end_measurement();
        if (stop_capture_after_measurement) {
            controller.stop_capture();
            capture_running = false;
        }
    } catch (...) {
        if (capture_running) {
            try {
                controller.stop_capture();
            } catch (...) {
            }
        }
        recorder.stop();
        throw;
    }

    const Stats stats = recorder.stop();
    const double capture_elapsed =
        std::chrono::duration<double>(measurement_end - measurement_start).count();
    const double observed_elapsed = stats.has_packets
        ? std::chrono::duration<double>(stats.last_packet - stats.first_packet).count()
        : 0.0;
    return {{"wlc_enabled", wlc},
            {"requested_windows", windows},
            {"asic_requested_windows",
             wlc ? nalu_board_controller::wlc_asic_windows_for_returned(windows) : windows},
            {"active_channels", channels},
            {"duration_seconds", duration_seconds},
            {"capture_elapsed_seconds", capture_elapsed},
            {"measurement_start_offset_ns", recorder.elapsed_ns(measurement_start)},
            {"measurement_end_offset_ns", recorder.elapsed_ns(measurement_end)},
            {"observed_packet_span_seconds", observed_elapsed},
            {"udp_datagrams", stats.datagrams},
            {"payload_bytes", stats.payload_bytes},
            {"packets_total", stats.packets},
            {"trigger_groups_total", stats.trigger_groups},
            {"anchor_channel", stats.anchor_channel},
            {"malformed_bytes", stats.malformed_bytes},
            {"payload_bytes_per_second",
             capture_elapsed > 0 ? stats.payload_bytes / capture_elapsed : 0.0},
            {"packets_per_second", capture_elapsed > 0 ? stats.packets / capture_elapsed : 0.0},
            {"trigger_groups_per_second",
             capture_elapsed > 0 ? stats.trigger_groups / capture_elapsed : 0.0},
            {"channel_packets", channel_map_json(stats.channel_packets)},
            {"channel_trigger_groups", channel_map_json(stats.channel_trigger_groups)},
            {"logical_positions", channel_map_json(stats.logical_positions)},
            {"raw_output_path", raw_output_path},
            {"raw_file_bytes", stats.raw_file_bytes},
            {"raw_event_limit", max_raw_events},
            {"raw_event_limit_reached", stats.raw_event_limit_reached}};
}

}  // namespace firmware_959_scan
