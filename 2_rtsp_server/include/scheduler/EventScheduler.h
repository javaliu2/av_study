#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <stdint.h>
#include "Timer.h"
#include "Event.h"

class Poller;

class EventScheduler {
public:
    enum PollerType {
        POLLER_SELECT,
        POLLER_POLL,
        POLLER_EPOLL
    };
    static EventScheduler* createNew(PollerType type);
    explicit EventScheduler(PollerType type);
    virtual ~EventScheduler();
public:
    bool addTriggerEvent(TriggerEvent* event);
    Timer::TimerId addTimerEventRunAfter(TimerEvent* event, Timer::TimeInterval delay);
    Timer::TimerId addTimerEventRunAt(TimerEvent* event, Timer::Timestamp when);
    Timer::TimerId addTimerEventRunEvery(TimerEvent* event, Timer::TimeInterval interval);
    bool removeTimerEvent(Timer::TimerId timerId);
    bool addIOEvent(IOEvent* event);
    bool updateIOEvent(IOEvent* event);
    bool removeIOEvent(IOEvent* event);

    void loop();
    Poller* poller();
    void setTimerManagerReadCallback(EventCallback cb, void* arg);

private:
    void handleTriggerEvents();

private:
    bool mQuit;
    Poller* mPoller;  // 成员变量的规范命名
    TimerManager* mTimerManager;
    std::vector<TriggerEvent*> mTriggerEvents;

    std::mutex mMtx;
    // WIN系统专用的定时器回调
    EventCallback mTimerManagerReadCallback;
    void* mTimerManagerArg;
};