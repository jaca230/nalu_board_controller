#include "ip_address_info.h"
#include <arpa/inet.h>
#include <stdexcept>

IPAddressInfo::IPAddressInfo(const std::string& ip_str, int port_num) : ip(ip_str), port(port_num) {
    if (!isValid()) {
        throw std::invalid_argument("Invalid IP or port.");
    }
}

IPAddressInfo::IPAddressInfo(const std::string& ip_port_str) {
    size_t pos = ip_port_str.find(':');
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid IP format. Expected format: 'IP:PORT'");
    }

    ip = ip_port_str.substr(0, pos);
    port = std::stoi(ip_port_str.substr(pos + 1));

    if (!isValid()) {
        throw std::invalid_argument("Invalid IP or port.");
    }
}

bool IPAddressInfo::isPortValid(int port) const {
    return port > 0 && port <= 65535;
}

bool IPAddressInfo::isIpValid(const std::string& ip) const {
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1 ||
           inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) == 1;
}

bool IPAddressInfo::isValid() const {
    return isIpValid(ip) && isPortValid(port);
}
