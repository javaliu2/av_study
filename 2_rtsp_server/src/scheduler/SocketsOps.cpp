#include "SocketsOps.h"
#include <fcntl.h>
#include <sys/types.h>
#ifndef _WIN32  // 编译器定义的宏
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/tcp.h>
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

bool sockets::bind(int sockfd, std::string ip, uint16_t port) {
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    if (::bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("::bind error, fd=%d, ip=%s, port=%d", sockfd, ip.c_str(), port);
        return false;
    }
    return true;
}

bool sockets::listen(int sockfd, int backlog) {
    if (::listen(sockfd, backlog) < 0) {
        LOG_ERROR("::listen error, fd=%d, backlog=%d", sockfd, backlog);
        return false;
    }
    return true;
}

int sockets::accept(int sockfd) {
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int connfd = ::accept(sockfd, (struct sockaddr*)&addr, &addrlen);
    setNonBlockAndCloseOnExec(connfd);
    ignoreSigPipeOnSocket(connfd);
    return connfd;
}

int sockets::write(int sockfd, const void* buf, int size) {
#ifndef _WIN32
    return ::write(sockfd, buf, size);
#endif
    return ::send(sockfd, (char*)buf, size, 0);
}

int sockets::sendto(int sockfd, const void* buf, int len,
        const struct sockaddr* destAddr) {
    socklen_t addrLen = sizeof(struct sockaddr);
    return ::sendto(sockfd, (char*)buf, len, 0, destAddr, addrLen);
}

int sockets::setNonBlock(int sockfd) {
#ifndef _WIN32
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    return 0;
#else
    unsigned long ul = 1;
    int ret = ioctlsocket(sockfd, FIONBIO, (unsigned long*)&ul);
    if (ret == SOCKET_ERROR) {
        return -1;
    } else {
        return 0; 
    }
#endif
}

int sockets::setBlock(int sockfd, int writeTimeout) {
#ifndef _WIN32
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));

    if (writeTimeout > 0) {
        struct timeval tv = {writeTimeout / 1000, (writeTimeout % 1000) * 1000};  // 毫秒、微秒
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
    }
#endif
    return 0;
}

void sockets::setReuseAddr(int sockfd, int on)
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
}

void sockets::setReusePort(int sockfd)
{
#ifdef SO_REUSEPORT
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&on, sizeof(on));
#endif
}

void sockets::setNonBlockAndCloseOnExec(int sockfd) {
#ifndef _WIN32
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
#endif
}

void sockets::ignoreSigPipeOnSocket(int socketfd)
{
#ifndef WIN32
    int option = 1;
    setsockopt(socketfd, SOL_SOCKET, MSG_NOSIGNAL, &option, sizeof(option));
#endif
}

void sockets::setNoDelay(int sockfd) {
#ifdef TCP_NODELAY
    int on = 1;
    int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on));
    // 关闭Nagle算法，让数据立刻发送，不做小包合并
#endif
}

void sockets::setKeepAlive(int sockfd) {
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));
    // 开启TCP KeepAlive(保活机制)，用于检测“死连接”
    // 有三个参数，分别是开始探测时间、探测间隔、探测次数
    // linux的默认参数值为7200秒、75秒、9次
    // 这显然不符合常规系统逻辑
    // 所以在工程上设置合理的参数非常必要
#ifndef _WIN32
    int idle = 60;  // 60秒后开始探测(保持多久空闲)
    int intvl = 10;  // 每10秒探测一次
    int cnt = 3;  // 连续探测3次
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
#endif
}

void sockets::setNoSigPipe(int sockfd) {
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (char*)&on, sizeof(on));
    // 防止向已关闭socket写数据时触发SIGPIPE信号
#endif
}

void sockets::setSendBufSize(int sockfd, int size) {
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}

void sockets::setRecvBufSize(int sockfd, int size) {
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
}

std::string sockets::getPeerIp(int sockfd) {
    struct sockaddr_in addr= {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if (getpeername(sockfd, (struct sockaddr*)&addr, &addrlen) == 0) {
        return inet_ntoa(addr.sin_addr);
    }
    return "0.0.0.0";
}

uint16_t sockets::getPeerPort(int sockfd) {
    struct sockaddr_in addr= {0};
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if (getpeername(sockfd, (struct sockaddr*)&addr, &addrlen) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

int sockets::getPeerAddr(int sockfd, struct sockaddr_in* addr) {
    socklen_t addrlen = sizeof(struct sockaddr_in);
    return getpeername(sockfd, (struct sockaddr*)addr, &addrlen);
}

void sockets::close(int sockfd) {
#ifndef _WIN32
    ::close(sockfd);
#else
    ::closesocket(sockfd);
#endif
}

bool sockets::connect(int sockfd, std::string ip, uint16_t port, int timeout) {
    if (timeout > 0) {
        sockets::setNonBlock(sockfd);
    }
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int ret = ::connect(sockfd, (struct sockaddr*)&addr, addrlen);
    if (ret == 0) {
        return true;
    }
    if (timeout <= 0) {
        return false;
    }
    if (errno != EINPROGRESS) {
        return false;
    }

    fd_set fdWrite;
    FD_ZERO(&fdWrite);
    FD_SET(sockfd, &fdWrite);

    struct timeval tv = {timeout / 1000, (timeout % 1000) * 1000};
    ret = select(sockfd + 1, NULL, &fdWrite, NULL, &tv);
    if (ret <= 0) {
        return false;
    }
    // socket上可写，可能是连接建立成功或者失败
    if (FD_ISSET(sockfd, &fdWrite)) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err == 0) {
            sockets::setBlock(sockfd, 0);
            return true;
        }
    } 
    return false;
}

std::string sockets::getLocalIp() {
    return "0.0.0.0";
}