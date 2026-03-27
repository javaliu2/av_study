#include "SocketsOps.h"
#include <fcntl.h>
#include <sys/types.h>
#ifndef _WIN32  // 编译器定义的宏
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif
#include "Logger.h"

int sockets::createTcpSock() {
#ifndef _WIN32
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOAK | SOCK_CLOEXEC, IPPROTO_TCP);
    // SOCK_CLOEXEC: close on exec，即在进程退出之后，自动关闭该sockfd。
    // 父进程fork子进程的话，子进程会继承父进程的fd，如果不设置SOCK_CLOEXEC，那么父进程exec之后
    // 其创建的{sockfd}，被子进程持有，还没有被释放。出现1）端口无法释放；2）连接泄露；3）安全问题等
#endif
    int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    unsigned long ul = 1;
    int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long*)&ul);
    if (ret == SOCKET_ERROR) {
        LOG_ERROR("设置socketfd非阻塞失败");
    }
    return sockfd;
}

int sockets::createUdpSock() {
#ifndef _WIN32
    int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
#endif
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    unsigned long ul = 1;
    int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long*)&ul);

    if (ret == SOCKET_ERROR) {
        LOG_ERROR("设置sockfd非阻塞失败");
    }
    return sockfd;
}


