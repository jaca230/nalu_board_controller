#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "nalu_board_controller/logging/logging.h"

namespace {

using json = nlohmann::json;

std::atomic<bool> running(true);

struct ReceiverConfig {
    std::string address = "192.168.1.1";
    uint16_t port = 12345;
    size_t max_packet_size = 1040;
    int timeout_sec = 2;
    size_t udp_header_bytes = 16;
    bool validate_payload_size = true;
};

struct ParserConfig {
    size_t packet_size = 74;
    std::string start_marker = "0E";
    std::string stop_marker = "FA5A";
    uint8_t chan_mask = 0x3F;
    uint8_t chan_shift = 0;
    uint8_t abs_wind_mask = 0x3F;
    uint8_t evt_wind_mask = 0x3F;
    uint8_t evt_wind_shift = 6;
    uint16_t timing_mask = 0x0FFF;
    uint8_t timing_shift = 12;
    bool check_packet_integrity = true;
};

struct PacketInfo {
    uint64_t index = 0;
    uint8_t channel = 0;
    uint32_t trigger_time = 0;
    uint16_t logical_position = 0;
    uint16_t physical_position = 0;
    std::array<uint8_t, 64> raw_samples{};
};

class PacketParser {
public:
    explicit PacketParser(ParserConfig config)
        : config_(std::move(config)),
          start_marker_(hex_to_bytes(config_.start_marker)),
          stop_marker_(hex_to_bytes(config_.stop_marker)) {}

    void append_payload(const uint8_t* data, size_t size) {
        buffer_.insert(buffer_.end(), data, data + size);
    }

    std::vector<PacketInfo> parse_available_packets() {
        std::vector<PacketInfo> packets;
        size_t offset = 0;

        while (offset + config_.packet_size <= buffer_.size()) {
            if (!config_.check_packet_integrity ||
                (check_marker(offset, start_marker_) &&
                 check_marker(offset + config_.packet_size - stop_marker_.size(), stop_marker_))) {
                packets.push_back(parse_packet(offset));
                offset += config_.packet_size;
                continue;
            }

            ++offset;
        }

        if (offset > 0) {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        return packets;
    }

private:
    static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
        if (hex.size() % 2 != 0) {
            throw std::runtime_error("Hex marker must have even length");
        }

        std::vector<uint8_t> bytes;
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2) {
            bytes.push_back(static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        }
        return bytes;
    }

    bool check_marker(size_t index, const std::vector<uint8_t>& marker) const {
        if (index + marker.size() > buffer_.size()) {
            return false;
        }

        for (size_t i = 0; i < marker.size(); ++i) {
            if (buffer_[index + i] != marker[i]) {
                return false;
            }
        }
        return true;
    }

    PacketInfo parse_packet(size_t offset) {
        PacketInfo packet;
        packet.index = packet_index_++;

        size_t cursor = offset + start_marker_.size();
        packet.channel = static_cast<uint8_t>((buffer_[cursor] >> config_.chan_shift) & config_.chan_mask);
        ++cursor;

        const uint16_t trigger_time_upper =
            static_cast<uint16_t>((buffer_[cursor] << 8) | buffer_[cursor + 1]);
        const uint16_t trigger_time_lower =
            static_cast<uint16_t>((buffer_[cursor + 2] << 8) | buffer_[cursor + 3]);
        packet.trigger_time =
            (static_cast<uint32_t>(trigger_time_upper) << config_.timing_shift) |
            (trigger_time_lower & config_.timing_mask);
        cursor += 4;

        packet.logical_position =
            ((buffer_[cursor] & config_.abs_wind_mask) << (8 - config_.evt_wind_shift)) |
            ((buffer_[cursor + 1] >> config_.evt_wind_shift) & config_.evt_wind_mask);
        packet.physical_position = buffer_[cursor + 1] & config_.abs_wind_mask;
        cursor += 2;

        std::copy_n(buffer_.begin() + static_cast<std::ptrdiff_t>(cursor), 64, packet.raw_samples.begin());
        return packet;
    }

    ParserConfig config_;
    std::vector<uint8_t> start_marker_;
    std::vector<uint8_t> stop_marker_;
    std::vector<uint8_t> buffer_;
    uint64_t packet_index_ = 0;
};

void signal_handler(int signal) {
    (void)signal;
    running = false;
}

void print_help() {
    std::cout << "Usage: packet_listener [options]\n"
              << "Options:\n"
              << "  --config PATH   Path to config.json\n"
              << "  --help          Show this help message\n";
}

json load_json(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    json document;
    stream >> document;
    return document;
}

template <typename T>
void assign_if_present(const json& object, const char* key, T& value) {
    const auto it = object.find(key);
    if (it != object.end() && !it->is_null()) {
        value = it->get<T>();
    }
}

ReceiverConfig parse_receiver_config(const json& object) {
    ReceiverConfig config;
    assign_if_present(object, "address", config.address);
    assign_if_present(object, "port", config.port);
    assign_if_present(object, "max_packet_size", config.max_packet_size);
    assign_if_present(object, "timeout_sec", config.timeout_sec);
    assign_if_present(object, "udp_header_bytes", config.udp_header_bytes);
    assign_if_present(object, "validate_payload_size", config.validate_payload_size);
    return config;
}

ParserConfig parse_parser_config(const json& object) {
    ParserConfig config;
    assign_if_present(object, "packet_size", config.packet_size);
    assign_if_present(object, "start_marker", config.start_marker);
    assign_if_present(object, "stop_marker", config.stop_marker);
    assign_if_present(object, "chan_mask", config.chan_mask);
    assign_if_present(object, "chan_shift", config.chan_shift);
    assign_if_present(object, "abs_wind_mask", config.abs_wind_mask);
    assign_if_present(object, "evt_wind_mask", config.evt_wind_mask);
    assign_if_present(object, "evt_wind_shift", config.evt_wind_shift);
    assign_if_present(object, "timing_mask", config.timing_mask);
    assign_if_present(object, "timing_shift", config.timing_shift);
    assign_if_present(object, "check_packet_integrity", config.check_packet_integrity);
    return config;
}

int create_socket(const ReceiverConfig& config) {
    const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    int reuse_address = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config.port);
    if (inet_pton(AF_INET, config.address.c_str(), &address.sin_addr) != 1) {
        close(socket_fd);
        throw std::runtime_error("Invalid bind address: " + config.address);
    }

    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(socket_fd);
        throw std::runtime_error("Failed to bind UDP socket");
    }

    if (config.timeout_sec > 0) {
        timeval timeout{};
        timeout.tv_sec = config.timeout_sec;
        timeout.tv_usec = 0;
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    return socket_fd;
}

void print_packet(const PacketInfo& packet) {
    std::ostringstream samples;
    samples << std::hex << std::setfill('0');
    for (size_t i = 0; i < 4; ++i) {
        if (i > 0) {
            samples << ' ';
        }
        samples << "0x" << std::setw(2) << static_cast<int>(packet.raw_samples[i]);
    }

    spdlog::info(
        "packet={} channel={} trigger_time={} logical_position={} physical_position={} first_bytes=[{}]",
        packet.index,
        static_cast<int>(packet.channel),
        packet.trigger_time,
        packet.logical_position,
        packet.physical_position,
        samples.str());
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::string config_path = "config.json";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                print_help();
                return 0;
            }
            if (arg == "--config") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("--config requires a value");
                }
                config_path = argv[++i];
                continue;
            }
            throw std::runtime_error("Unknown argument: " + arg);
        }

        const json document = load_json(config_path);
        const json logging_config = document.value("logging", json::object());
        const auto receiver_config = parse_receiver_config(document.at("receiver"));
        const auto parser_config = parse_parser_config(document.at("parser"));

        nalu_board_controller::logging::configure(logging_config.value("level", "info"));
        std::signal(SIGINT, signal_handler);

        PacketParser parser(parser_config);
        std::vector<uint8_t> datagram(receiver_config.max_packet_size);

        const int socket_fd = create_socket(receiver_config);
        spdlog::info(
            "Listening on {}:{} using config '{}'",
            receiver_config.address,
            receiver_config.port,
            config_path);

        while (running) {
            sockaddr_in client{};
            socklen_t client_length = sizeof(client);
            const ssize_t bytes_received =
                recvfrom(socket_fd, datagram.data(), datagram.size(), 0, reinterpret_cast<sockaddr*>(&client), &client_length);

            if (bytes_received < 0) {
                continue;
            }

            if (static_cast<size_t>(bytes_received) <= receiver_config.udp_header_bytes) {
                spdlog::warn("Ignoring undersized UDP datagram ({} bytes)", bytes_received);
                continue;
            }

            if (receiver_config.validate_payload_size) {
                uint16_t payload_size_network = 0;
                std::memcpy(&payload_size_network, datagram.data(), sizeof(payload_size_network));
                const uint16_t payload_size = ntohs(payload_size_network);
                const size_t payload_bytes =
                    static_cast<size_t>(bytes_received) - receiver_config.udp_header_bytes;
                if (payload_size != payload_bytes) {
                    spdlog::warn(
                        "Skipping malformed datagram: declared payload={} actual_payload={}",
                        payload_size,
                        payload_bytes);
                    continue;
                }
            }

            parser.append_payload(
                datagram.data() + static_cast<std::ptrdiff_t>(receiver_config.udp_header_bytes),
                static_cast<size_t>(bytes_received) - receiver_config.udp_header_bytes);

            for (const auto& packet : parser.parse_available_packets()) {
                print_packet(packet);
            }
        }

        close(socket_fd);
        spdlog::info("Listener stopped.");
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("packet_listener failed: {}", error.what());
        return 1;
    }
}
