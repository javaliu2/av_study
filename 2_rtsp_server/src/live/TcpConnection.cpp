#include "live/TcpConnection.h"
#include "scheduler/SocketsOps.h"
#include "base/Logger.h"

TcpConnection::TcpConnection(UsageEnvironment* env, int clientFd) :
    mEnv(env), mClientFd(clientFd) {
    mClientIOEvent = IOEvent::createNew(clientFd, this);
    mClientIOEvent->setReadCallback(readCallback);
    mClientIOEvent->setWriteCallback(writeCallback);
    mClientIOEvent->setErrorCallback(errorCallback);
    mClientIOEvent->enableReadHandling();  // 默认只开启对可读事件的监听

    mEnv->scheduler()->addIOEvent(mClientIOEvent);
}

TcpConnection::~TcpConnection() {
    mEnv->scheduler()->removeIOEvent(mClientIOEvent);
    delete mClientIOEvent;
    sockets::close(mClientFd);
}

void TcpConnection::setDisConnectCallback(DisConnectCallback cb, void* arg) {
    mDisConnectCallback = cb;
    mArg = arg;
}

void TcpConnection::enableErrorHandling() {
    // 支持可重入
    if (mClientIOEvent->isErrorHandling()) {
        return;
    }
    mClientIOEvent->enableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::enableReadHandling() {
    if (mClientIOEvent->isReadHandling()) {
        return;
    }
    mClientIOEvent->enableReadHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::enableWriteHandling() {
    if (mClientIOEvent->isWriteHandling()) {
        return;
    }
    mClientIOEvent->enableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableReadHandling() {
    if (!mClientIOEvent->isReadHandling()) {
        return;
    }
    mClientIOEvent->disableReadHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableWriteHandling() {
    if (!mClientIOEvent->isWriteHandling()) {
        return;
    }
    mClientIOEvent->disableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}

void TcpConnection::disableErrorHandling() {
    if (!mClientIOEvent->isErrorHandling()) {
        return;
    }
    mClientIOEvent->disableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mClientIOEvent);
}
// 连接上的可读事件（就是有请求到达了，有新数据）回调函数
void TcpConnection::handleRead() {
    int ret = mInputBuffer.read(mClientFd);
    if (ret <= 0) {
        LOG_ERROR("read error, fd=%d, ret=%d", mClientFd, ret);
        handleDisConnect();
        return;
    }
    handleReadBytes();  // 调用RtspConnection类的实现函数
}

void TcpConnection::handleReadBytes() {
    LOG_DEBUG("TcpConnection::handleReadBytes()");
    mInputBuffer.retrieveAll();  // what mean? 直接将缓冲区清空，不对数据进行业务处理
}

void TcpConnection::handleDisConnect() {
    if (mDisConnectCallback) {
        mDisConnectCallback(mArg, mClientFd);
    }
}

void TcpConnection::handleWrite() {
    LOG_INFO("");
    mOutBuffer.retrieveAll();  // TODO the same as above
}

void TcpConnection::handleError() {
    LOG_INFO("");
}

// 连接上的读事件 设置回调函数
void TcpConnection::readCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleRead();
}

void TcpConnection::writeCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleWrite();
}

void TcpConnection::errorCallback(void* arg) {
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleError();
}



