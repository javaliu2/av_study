#include "EventScheduler.h"
#include "SocketsOps.h"
#include "SelectPoller.h"
#include "Logger.h"

#ifndef WIN32
#include <sys/eventfd.h>
#endif

EventScheduler* EventScheduler::createNew(PollerType type) {
    if (type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL) {
        return NULL;
    }
    return new EventScheduler(type);
}

EventScheduler::EventScheduler(PollerType type) : mQuit(false) {
#ifdef WIN32
    WSADATA wdSockMsg;
    int s = WSAStartup(MAKEWORD(2, 2), &wdSockMsg);
    if (0 != s) {
        switch (s) {
            case WSASYSNOTREADY:
                LOG_ERROR("重启电脑，或者检查网络库");
                break;
            case WSAVERNOTSUPPORTED:
                LOG_ERROR("请更新网络库");
                break;
            case WSAEINPROGRESS:
                LOG_ERROR("请重新启动");
                break;
            case WSAEPROCLIM:
                LOG_ERROR("请关闭不必要的软件，以确保有足够的网络资源");
                break;
        }
    }
    if (2 != HIBYTE(wdSockMsg.wVersion) || 2 != LOBYTE(wdSockMsg.wVersion)) {
        LOG_ERROR("网络库版本错误");
        return;
    }
    switch (type) {
        case POLLER_SELECT:
            mPoller = SelectPoller::createNew();
            break;
        case POLLER_POLL:
            // mPoller = PollPoller::createNew();  // 未提供实现
            break;
        case POLLER_EPOLL:
            // mPoller = EpollPoller::createNew();
            break;
        default:
            _exit(-1);
            break;
    }
    mTimerManager = TimerManager::createNew(this);  // WIN系统的定时器回调由子线程托管，非WIN系统则通过select网络模型
#endif
}