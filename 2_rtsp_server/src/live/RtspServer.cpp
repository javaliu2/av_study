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
    mAcceptIOEvent->setReadCallback(acceptCallback);  // 设置 监听socket 的可读事件（即有新的client连接）
    mAcceptIOEvent->enableReadHandling();

    mCloseTriggerEvent = TriggerEvent::createNew(this);
    mCloseTriggerEvent->setTriggerCallback(closeConnectCallback);  // 设置 连接关闭 的回调函数
}

// 开启服务器监听，同时将mAcceptIOEvent加入poller监听
void RtspServer::start() {
    LOG_INFO("RtspServer::start()");
    mListen = true;
    sockets::listen(mFd, 60);
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);  // 将 接受新连接 事件加入poller
}

// ==== 接受新连接的 回调函数 和 回调处理函数  ====
void RtspServer::acceptCallback(void* arg) {
    RtspServer* server = (RtspServer*)arg;
    server->handleAccept();
}

void RtspServer::handleAccept() {
    int clientFd = sockets::accept(mFd);
    if (clientFd < 0) {
        LOG_ERROR("handleAccept error, clientFd=%d", clientFd);
        return;
    }
    LOG_INFO("new client connected, clientFd=%d", clientFd);
    RtspConnection* rtspConnection = RtspConnection::createNew(this, clientFd);
    rtspConnection->setDisConnectCallback(RtspServer::disConnectCallback, this);  // 连接断开的通知事件
    mConnMap.insert(std::make_pair(clientFd, rtspConnection));
}
// ==== 连接断开的通知事件的 回调函数 和 回调处理函数 ====
void RtspServer::disConnectCallback(void* arg, int clientFd) {
    RtspServer* server = (RtspServer*)arg;
    server->handleDisConnect(clientFd);
}
void RtspServer::handleDisConnect(int clientFd) {
    // Q: 为什么要使用锁
    // A: 保护mDisConnList共享变量的并发修改
    // 但是在这里其实没有涉及到多线程，main线程执行eventScheduler的loop函数，
    // 该函数中detach了一个定时器事件处理线程，对IO事件和trigger事件的处理是在main线程做的
    // server启动会注册accept事件监听，在其处理函数中创建RtspConnection对象，该对象调用父类TcpConnection的构造方法，
    // 该方法注册了rtsp连接的可读事件监听，如果可读事件出现了连接请求断开的情况，那么会调用handleDisConnect函数，
    // 对该函数的调用是在main线程中做的，添加完trigger事件后，main线程会继续执行eventScheduler的loop函数，在该函数中会调用trigger事件的回调函数cbCloseConnect，
    // 所以这里不涉及到多个线程，
    // io事件的处理没有使用线程池，rtsp连接的可读事件也是main线程，所以这里的锁互斥访问是没有必要的
    // 加上也没毛病
    LOG_DEBUG("for testing multi-thread or not");
    std::lock_guard<std::mutex> lck(mMtx);  // lock1
    mDisConnList.push_back(clientFd);
    mEnv->scheduler()->addTriggerEvent(mCloseTriggerEvent);
}

// ==== 关闭连接事件的 设置回调函数 和 回调函数
void RtspServer::closeConnectCallback(void* arg) {
    RtspServer* server = (RtspServer*)arg;
    server->handleCloseConnect();
}

void RtspServer::handleCloseConnect() {
    LOG_DEBUG("for testing multi-thread or not");
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
