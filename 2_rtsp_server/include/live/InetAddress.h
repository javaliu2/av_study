#pragma once
#include <string>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif
#include <cstdint>

class Ipv4Address {
public:
    Ipv4Address();
    Ipv4Address(std::string ip, uint16_t port);
    void setAddr(std::string ip, uint16_t port);
    std::string getIp() const;
    uint16_t getPort() const;
    struct sockaddr* getAddr() const;
private:
    std::string mIp;
    uint16_t mPort;
    struct sockaddr_in mAddr;
};