#ifndef IP_ADDRESS_INFO_H
#define IP_ADDRESS_INFO_H

#include <string>
#include <stdexcept>

class IPAddressInfo {
public:
    // Default constructor
    IPAddressInfo() : ip(""), port(0) {}

    // Constructor that takes a string and port number
    IPAddressInfo(const std::string& ip_str, int port_num);

    // Constructor that takes combined IP:PORT string
    IPAddressInfo(const std::string& ip_port_str);

    // Get the IP part of the address
    std::string getIp() const { return ip; }

    // Get the port part of the address
    int getPort() const { return port; }

    // Get the combined string representation "IP:PORT"
    std::string getCombined() const { return ip + ":" + std::to_string(port); }

    bool isSet() const { return !ip.empty() && port != 0; }  // Check if both IP and port are set

private:
    std::string ip;
    int port;

    // Private methods for validation
    bool isPortValid(int port) const;
    bool isIpValid(const std::string& ip) const;
    bool isValid() const;
};

#endif // IP_ADDRESS_INFO_H
