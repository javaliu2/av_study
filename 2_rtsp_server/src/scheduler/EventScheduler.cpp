#include "scheduler/EventScheduler.h"
#include "scheduler/SocketsOps.h"
#include "scheduler/SelectPoller.h"
#include "base/Logger.h"
#include <thread>
#ifndef WIN32
#include <sys/eventfd.h>
#endif
/**
 * note: 从这些方法的实现可以体现出该类名为 EventScheduler 的含义，即事件调度
 * 负责mTimerManager、mPoller、WSADATA的创建、释放
 */
EventScheduler* EventScheduler::createNew(PollerType type) {
    if (type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL) {
        return NULL;
    }
    return new EventScheduler(type);
}
// 初始化 mPoller 对象和 mTimerManager 对象
EventScheduler::EventScheduler(PollerType type) : mQuit(false) {
#ifdef WIN32
    WSADATA wdSockMsg;  // WSA: Windows Sockets Api
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
    // 初始化mTimerManager对象，同时设置this对象，即当前EventScheduler对象的mTimerManagerReadCallback为TimerManager对象的readCallback函数
    mTimerManager = TimerManager::createNew(this);  // WIN系统的定时器回调由子线程托管，非WIN系统则通过select网络模型
#endif
}

EventScheduler::~EventScheduler() {
    delete mTimerManager;
    delete mPoller;

#ifdef WIN32
    WSACleanup();
#endif
}

bool EventScheduler::addTriggerEvent(TriggerEvent* event) {
    mTriggerEvents.push_back(event);
    return true;
}

Timer::TimerId EventScheduler::addTimerEventRunAfter(TimerEvent* event, Timer::TimeInterval delay) {
    Timer::Timestamp timestamp = Timer::getCurTime();
    timestamp += delay;
    return mTimerManager->addTimer(event, timestamp, 0);
}

Timer::TimerId EventScheduler::addTimerEventRunAt(TimerEvent* event, Timer::Timestamp when) {
    return mTimerManager->addTimer(event, when, 0);
}

Timer::TimerId EventScheduler::addTimerEventRunEvery(TimerEvent* event, Timer::TimeInterval interval) {
    Timer::Timestamp timestamp = Timer::getCurTime();
    timestamp += interval;
    return mTimerManager->addTimer(event, timestamp, interval);
}

bool EventScheduler::removeTimerEvent(Timer::TimerId timerId) {
    return mTimerManager->removeTimer(timerId);
}

bool EventScheduler::addIOEvent(IOEvent* event) {
    return mPoller->addIOEvent(event);
}

bool EventScheduler::updateIOEvent(IOEvent* event) {
    return mPoller->updateIOEvent(event);
}

bool EventScheduler::removeIOEvent(IOEvent* event) {
    return mPoller->removeIOEvent(event);
}

void EventScheduler::loop() {
#ifdef WIN32
    // std::thread([](EventScheduler* sch) {
    //     while (!sch->mQuit) {
    //         if (sch->mTimerManagerReadCallback) {
    //             sch->mTimerManagerReadCallback(sch->mTimerManagerArg);
    //         }
    //     }
    // }, this).detach();  // 1st: 匿名函数a，接受一个EventScheduler*类型的参数；2nd: 传递给匿名函数a的参数
    // detach: 让该线程脱离主线程，自己运行；
    // 存在的风险: this指向的EventScheduler对象析构之后，匿名函数对其成员变量的访问会出现NPE
    // G-bro建议这样写:
    std::thread([this]() {
        while (!mQuit) {
            // 在初始化当前对象的时候已经完成了对 mTimerManagerReadCallback 和 mTimerManagerArg 的赋值
            if (mTimerManagerReadCallback) {
                mTimerManagerReadCallback(mTimerManagerArg);
            }
        }
    }).detach();  // 单独的线程处理timer事件
#endif
    while (!mQuit) {
        handleTriggerEvents();  // 本线程处理trigger事件
        mPoller->handleEvent();  // 本线程处理poller事件
    }
}

void EventScheduler::handleTriggerEvents() {
    if (!mTriggerEvents.empty()) {
        for (std::vector<TriggerEvent*>::iterator it = mTriggerEvents.begin(); it != mTriggerEvents.end(); ++it) {
            (*it)->handleEvent();
        }
        mTriggerEvents.clear();
    }
}

Poller* EventScheduler::poller() {
    return mPoller;
}

void EventScheduler::setTimerManagerReadCallback(EventCallback cb, void* arg) {
    mTimerManagerReadCallback = cb;
    mTimerManagerArg = arg;
}
