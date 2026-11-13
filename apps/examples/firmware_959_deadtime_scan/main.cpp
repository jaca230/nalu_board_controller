#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "nalu_board_controller/config/board_config.h"
#include "nalu_board_controller/config/capture_config.h"
#include "nalu_board_controller/config/wlc_window_mapping.h"
#include "nalu_board_controller/controller/controller.h"
#include "nalu_board_controller/logging/logging.h"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

std::atomic<bool> interrupted{false};

void handle_signal(int) {
    interrupted = true;
}

struct Stats {
    uint64_t datagrams = 0;
    uint64_t payload_bytes = 0;
    uint64_t packets = 0;
    uint64_t malformed_bytes = 0;
    uint64_t events = 0;
    std::map<int, uint64_t> channel_packets;
    std::map<int, uint64_t> logical_positions;
    Clock::time_point first{};
    Clock::time_point last{};
    bool has_packets = false;
};

json read_json(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open config: " + path);
    json value;
    in >> value;
    return value;
}

class UdpCounter {
public:
    UdpCounter(std::string address, uint16_t port, size_t header_bytes)
        : address_(std::move(address)), port_(port), header_bytes_(header_bytes) {}

    void start() {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) throw std::runtime_error("socket() failed");
        timeval timeout{0, 200000};
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(port_);
        if (inet_pton(AF_INET, address_.c_str(), &endpoint.sin_addr) != 1 ||
            bind(socket_, reinterpret_cast<sockaddr*>(&endpoint), sizeof(endpoint)) < 0) {
            ::close(socket_);
            socket_ = -1;
            throw std::runtime_error("Unable to bind UDP receiver " + address_ + ":" + std::to_string(port_));
        }
        running_ = true;
        worker_ = std::thread([this] { receive(); });
    }

    Stats stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        if (socket_ >= 0) ::close(socket_);
        socket_ = -1;
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    ~UdpCounter() {
        if (running_) stop();
    }

private:
    void receive() {
        std::vector<uint8_t> datagram(65536);
        std::vector<uint8_t> stream;
        while (running_) {
            const ssize_t size = recv(socket_, datagram.data(), datagram.size(), 0);
            if (size <= 0) continue;
            const size_t payload_start = std::min(header_bytes_, static_cast<size_t>(size));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++stats_.datagrams;
                stats_.payload_bytes += static_cast<size_t>(size) - payload_start;
            }
            stream.insert(stream.end(), datagram.begin() + payload_start, datagram.begin() + size);
            size_t offset = 0;
            while (offset + 74 <= stream.size()) {
                if (stream[offset] != 0x0e || stream[offset + 72] != 0xfa || stream[offset + 73] != 0x5a) {
                    ++offset;
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++stats_.malformed_bytes;
                    continue;
                }
                const int channel = stream[offset + 1] & 0x3f;
                const int logical = ((stream[offset + 6] & 0x3f) << 2) |
                                    ((stream[offset + 7] >> 6) & 0x03);
                const auto now = Clock::now();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (!stats_.has_packets) stats_.first = now;
                    stats_.last = now;
                    stats_.has_packets = true;
                    ++stats_.packets;
                    ++stats_.channel_packets[channel];
                    ++stats_.logical_positions[logical];
                    if (logical == 0 && (anchor_channel_ < 0 || channel == anchor_channel_)) {
                        if (anchor_channel_ < 0) anchor_channel_ = channel;
                        ++stats_.events;
                    }
                }
                offset += 74;
            }
            if (offset) stream.erase(stream.begin(), stream.begin() + static_cast<long>(offset));
        }
    }

    std::string address_;
    uint16_t port_;
    size_t header_bytes_;
    int socket_ = -1;
    int anchor_channel_ = -1;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex mutex_;
    Stats stats_;
};

nalu_board_controller::BoardConfig board_config(const json& value) {
    nalu_board_controller::BoardConfig out;
    out.model = value.value("model", out.model);
    out.board_endpoint = value.value("board_endpoint", out.board_endpoint);
    out.host_endpoint = value.value("host_endpoint", out.host_endpoint);
    out.asic_serial_mode = value.value("asic_serial_mode", out.asic_serial_mode);
    return out;
}

nalu_board_controller::CaptureConfig capture_config(
    const json& root, bool wlc, int requested_windows, const std::vector<int>& active_channels) {
    const auto& value = root.at("capture");
    nalu_board_controller::CaptureConfig out;
    out.target_endpoint = value.value("target_endpoint", out.target_endpoint);
    out.assign_dac_values = value.value("assign_dac_values", false);
    out.readoutchannels_register = value.value("readoutchannels_register", -1);
    out.readout_window.windows = requested_windows;
    out.readout_window.lookback = requested_windows;
    out.readout_window.write_after_trigger = requested_windows;
    out.readout_window.lookback_mode = value.value("lookback_mode", "trigrel");
    out.trigger.mode = value.value("trigger_mode", "self");
    out.trigger.low_reference = value.value("low_reference", 5);
    out.trigger.high_reference = value.value("high_reference", 11);
    out.trigger.rising_edge = value.value("rising_edge", true);
    out.window_level_control.enabled = wlc;
    out.window_level_control.reinitialize_after_change = true;
    const int channel_count = value.value("channel_count", 32);
    const int trigger_value = value.value("trigger_value", 1);
    const int dac_value = value.value("dac_value", 1804);
    for (int channel : active_channels) {
        if (channel < 0 || channel >= channel_count) throw std::runtime_error("Invalid active channel");
    }
    for (int channel = 0; channel < channel_count; ++channel) {
        const bool enabled =
            std::find(active_channels.begin(), active_channels.end(), channel) != active_channels.end();
        out.channels[channel] = {enabled, trigger_value, dac_value};
    }
    return out;
}

json measure(nalu_board_controller::Controller& controller,
             const json& document,
             bool wlc,
             int windows,
             const std::vector<int>& channels,
             double duration,
             bool configure_capture) {
    auto capture = capture_config(document, wlc, windows, channels);
    const auto& receiver = document.at("receiver");
    UdpCounter counter(receiver.value("address", "192.168.1.1"),
                       receiver.value("port", 12345),
                       receiver.value("udp_header_bytes", 16));
    counter.start();
    if (configure_capture) controller.start_capture(capture);
    else controller.resume_capture();
    const auto deadline = Clock::now() + std::chrono::duration<double>(duration);
    while (!interrupted && Clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    controller.stop_capture();
    const Stats stats = counter.stop();
    const double elapsed = stats.has_packets
        ? std::chrono::duration<double>(stats.last - stats.first).count() : 0.0;
    return {
        {"wlc_enabled", wlc}, {"requested_windows", windows},
        {"asic_requested_windows",
         wlc ? nalu_board_controller::wlc_asic_windows_for_returned(windows) : windows},
        {"active_channels", channels},
        {"duration_seconds", duration}, {"observed_duration_seconds", elapsed},
        {"udp_datagrams", stats.datagrams}, {"payload_bytes", stats.payload_bytes},
        {"packets_total", stats.packets}, {"events_total", stats.events},
        {"malformed_bytes", stats.malformed_bytes},
        {"packets_per_second", elapsed > 0 ? stats.packets / elapsed : 0.0},
        {"events_per_second", elapsed > 0 ? stats.events / elapsed : 0.0},
        {"channel_packets", stats.channel_packets}, {"logical_positions", stats.logical_positions}
    };
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);
        std::string config_path;
        std::string output_path;
        bool serve = false;
        bool wlc = false;
        int windows = 0;
        double duration = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
            else if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
            else if (arg == "--windows" && i + 1 < argc) windows = std::stoi(argv[++i]);
            else if (arg == "--duration" && i + 1 < argc) duration = std::stod(argv[++i]);
            else if (arg == "--wlc" && i + 1 < argc) wlc = std::stoi(argv[++i]) != 0;
            else if (arg == "--serve") serve = true;
            else throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
        if (config_path.empty()) throw std::runtime_error("Required: --config PATH");

        const json document = read_json(config_path);
        nalu_board_controller::logging::configure(document.value("log_level", "info"));
        nalu_board_controller::Controller controller(board_config(document.at("board")));
        if (serve) {
            bool initialized = false;
            json previous;
            std::string line;
            while (!interrupted && std::getline(std::cin, line)) {
                if (line.empty()) continue;
                const json request = json::parse(line);
                const bool requested_wlc = request.at("wlc_enabled").get<bool>();
                const int requested_windows = request.at("requested_windows").get<int>();
                const auto channels = request.at("active_channels").get<std::vector<int>>();
                const double requested_duration = request.at("duration_seconds").get<double>();
                const json hardware_key = {{"wlc_enabled", requested_wlc},
                                           {"requested_windows", requested_windows},
                                           {"active_channels", channels}};
                if (!initialized) {
                    nalu_board_controller::WindowLevelControlConfig mode;
                    mode.enabled = requested_wlc;
                    controller.set_window_level_control(mode);
                    controller.initialize_board();
                    initialized = true;
                }
                const bool changed = previous.is_null() || previous != hardware_key;
                json result = measure(controller, document, requested_wlc, requested_windows,
                                      channels, requested_duration, changed);
                result["hardware_reconfigured"] = changed;
                previous = hardware_key;
                std::cout << "RESULT_JSON:" << result.dump() << std::endl;
            }
            return 0;
        }
        throw std::runtime_error("Use --serve (the scan driver manages requests)");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "firmware_959_deadtime_point failed: " << error.what() << '\n';
        return 1;
    }
}
