#ifndef NALU_BOARD_CONTROLLER_TYPES_ADDRESS_H
#define NALU_BOARD_CONTROLLER_TYPES_ADDRESS_H

#include <stdexcept>
#include <string>

namespace nalu_board_controller {

class Address {
public:
    Address() = default;
    Address(const std::string& ip, int port);
    explicit Address(const std::string& endpoint);

    const std::string& ip() const { return ip_; }
    int port() const { return port_; }
    std::string combined() const { return ip_ + ":" + std::to_string(port_); }
    bool is_set() const { return !ip_.empty() && port_ != 0; }

    const std::string& getIp() const { return ip(); }
    int getPort() const { return port(); }
    std::string getCombined() const { return combined(); }
    bool isSet() const { return is_set(); }

private:
    bool is_port_valid(int port) const;
    bool is_ip_valid(const std::string& ip) const;
    bool is_valid() const;

    std::string ip_;
    int port_ = 0;
};

}  // namespace nalu_board_controller

#endif
