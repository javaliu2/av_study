#pragma once
#include <string>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif
#include <cstdint>
/** 
 * Q: 这里为什么使用命名空间？
 * A: 
 * 1、将和socket有关的功能函数打包在一起
 * 2、避免与系统函数发生冲突
 * 3、my idea: 不好使用类，因为这里的操作对象有多个，fd，连接，数据的收发等
*/
namespace sockets {
    int createTcpSock();  // 默认创建非阻塞的tcp描述符
    int createUdpSock();  // 默认创建非阻塞的udp描述符
    bool bind(int sockfd, std::string ip, uint16_t port);
    bool listen(int sockfd, int backlog);
    int accept(int sockfd);
    int write(int sockfd, const void* buf, int size);  // tcp写入
    int sendto(int sockfd, const void* buf, int len, const struct sockaddr* destAddr);  // udp写入
    int setNonBlock(int sockfd);
    int setBlock(int sockfd);
    /**
     * Q: 地址重用和端口重用有什么区别？
     * A: 
     * 1、地址重用: 允许绑定处于TIME_WAIT状态的端口
     * 2、端口重用：允许多个进程/线程绑定同一个端口
     */
    void setReuseAddr(int sockfd, int on);  
    void setReusePort(int sockfd);
    void setNonBlockAndCloseOnExec(int sockfd);
    void ignoreSigPipeOnSocket(int sockfd);
    void setNoDelay(int sockfd);
    void setKeepAlive(int sockfd);
    void setNoSigPipe(int sockfd);
    void setSendBufSize(int sockfd, int size);
    void setRecvBufSize(int sockfd, int size);
    std::string getPeerIp(int sockfd);
    uint16_t getPeerPort(int sockfd);
    int getPeerAddr(int sockfd, struct sockaddr_in* addr);
    void close(int sockfd);
    bool connect(int sockfd, std::string ip, uint16_t port, int timeout);
    std::string getLocalIp();
}