#include "live/RtspServer.h"
#include "live/RtspConnection.h"
#include "base/Logger.h"

RtspServer* RtspServer::createNew(UsageEnvironment* env, MediaSessionManager* sessMgr, Ipv4Address& addr) {
    return new RtspServer(env, sessMgr, addr);
}

RtspServer::RtspServer(UsageEnvironment* env, MediaSessionManager* sessMgr, Ipv4Address& addr) :
    mSessMgr(sessMgr), mEnv(env), mAddr(addr), mListen(false) {
    mFd = sockets::createTcpSock();  // 创建服务器 监听socket
    sockets::setReuseAddr(mFd, 1);
    if (!sockets::bind(mFd, addr.getIp(), mAddr.getPort())) {
        return;
    }
    LOG_INFO("rtsp://%s:%d fd=%d", addr.getIp().data(), addr.getPort(), mFd);
    mAcceptIOEvent = IOEvent::createNew(mFd, this);
    mAcceptIOEvent->setReadCallback(readCallback);  // 设置 监听socket 的可读事件（即有新的client连接）
    mAcceptIOEvent->enableReadHandling();

    mCloseTriggerEvent = TriggerEvent::createNew(this);
    mCloseTriggerEvent->setTriggerCallback(cbCloseConnect);  // 设置 连接关闭 的回调函数
}

void RtspServer::start() {
    LOG_INFO("RtspServer::start()");
    mListen = true;
    sockets::listen(mFd, 60);
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);  // 将 接受新连接 事件加入poller
}

// ==== 接受新连接的 设置回调函数 和 回调函数  ====
void RtspServer::readCallback(void* arg) {
    RtspServer* server = (RtspServer*)arg;
    server->handleRead();
}

void RtspServer::handleRead() {
    int clientFd = sockets::accept(mFd);
    if (clientFd < 0) {
        LOG_ERROR("handleRead error, clientFd=%d", clientFd);
        return;
    }
    RtspConnection* rtspConnection = RtspConnection::createNew(this, clientFd);
    rtspConnection->setDisConnectCallback(RtspServer::cbDisConnect, this);  // 连接断开的通知事件
    mConnMap.insert(std::make_pair(clientFd, rtspConnection));
}
// ==== 连接断开的通知事件的 设置回调函数 和 回调函数 ====
void RtspServer::cbDisConnect(void* arg, int clientFd) {
    RtspServer* server = (RtspServer*)arg;
    server->handleDisConnect(clientFd);
}
void RtspServer::handleDisConnect(int clientFd) {
    // TODO 为什么要使用锁
    // 保护mDisConnList共享变量的并发修改
    std::lock_guard<std::mutex> lck(mMtx);  // lock1
    mDisConnList.push_back(clientFd);
    mEnv->scheduler()->addTriggerEvent(mCloseTriggerEvent);
}

// ==== 关闭连接事件的 设置回调函数 和 回调函数
void RtspServer::cbCloseConnect(void* arg) {
    RtspServer* server = (RtspServer*)arg;
    server->handleCloseConnect();
}

void RtspServer::handleCloseConnect() {
    std::lock_guard<std::mutex> lck(mMtx);  // lock2 和 lock1 是同一把锁（对象锁），因为是同一个mutex对象
    for (auto it = mDisConnList.begin(); it != mDisConnList.end(); ++it) {
        int clientFd = *it;
        auto _it = mConnMap.find(clientFd);
        assert(_it != mConnMap.end());
        delete _it->second;  // 关闭连接
        mConnMap.erase(clientFd);
    }
    mDisConnList.clear();
}
