#include "nalu_board_controller/types/address.h"

#include <arpa/inet.h>

namespace nalu_board_controller {

Address::Address(const std::string& ip, int port) : ip_(ip), port_(port) {
    if (!is_valid()) {
        throw std::invalid_argument("Invalid IP or port.");
    }
}

Address::Address(const std::string& endpoint) {
    const size_t separator = endpoint.find(':');
    if (separator == std::string::npos) {
        throw std::invalid_argument("Invalid IP format. Expected format: 'IP:PORT'");
    }

    ip_ = endpoint.substr(0, separator);
    port_ = std::stoi(endpoint.substr(separator + 1));

    if (!is_valid()) {
        throw std::invalid_argument("Invalid IP or port.");
    }
}

bool Address::is_port_valid(int port) const {
    return port > 0 && port <= 65535;
}

bool Address::is_ip_valid(const std::string& ip) const {
    sockaddr_in ipv4 {};
    sockaddr_in6 ipv6 {};
    return inet_pton(AF_INET, ip.c_str(), &ipv4.sin_addr) == 1 ||
           inet_pton(AF_INET6, ip.c_str(), &ipv6.sin6_addr) == 1;
}

bool Address::is_valid() const {
    return is_ip_valid(ip_) && is_port_valid(port_);
}

}  // namespace nalu_board_controller
