#pragma once
#include <map>
#include <stdint.h>

class EventScheduler;
class Poller;
class TimerEvent;
class IOEvent;

class Timer {
public:
    typedef uint32_t TimerId;
    /**
     * 问：为什么采用int64?
     * 答：因为时间的计算会出现负数。举例，当前时刻的时间戳now_timestamp为5，
     * 定时器开始工作的时间戳target_timestamp为6，那么
     * delta = now_timestamp - target_timestamp = 5 - 6 = -1，负数表示定时器开始工作的时间还没到。
     * 如果使用uint64，那么会出现下溢，出现crash
     */
    typedef int64_t Timestamp;  // ms
    typedef uint32_t TimeInterval;  // ms

    ~Timer();

    static Timestamp getCurTime();  // 获取当前系统启动以来的毫秒数
    static Timestamp getCurTimestamp();  // 获取毫秒级时间戳（13位）

private:
    friend class TimerManager;
    Timer(TimerEvent* event, Timestamp timestamp, TimeInterval timeInterval, TimerId timeId);

private:
    bool handleEvent();
private:
    TimerEvent* mTimerEvent;
    Timestamp mTimestamp;  // 时刻
    TimeInterval mTimeInterval;  // 时长
    TimerId mTimerId;

    bool mRepeat;
};

class TimerManager {
public:
    static TimerManager* createNew(EventScheduler* scheduler);

    TimerManager(EventScheduler* scheduler);
    ~TimerManager();

    Timer::TimerId addTimer(TimerEvent* event, Timer::Timestamp timestamp, Timer::TimeInterval timeInterval);
    bool removeTimer(Timer::TimerId timerId);
private:
    // Q: why set be 'static' ?
    // A: 设置回调函数一般都设置为static的，因为参数arg可以指明是哪一个对象
    static void readCallback(void* arg);  
    void handleRead();
    void modifyTimeout();
private:
    Poller* mPoller;
    std::map<Timer::TimerId, Timer> mTimers;
    std::multimap<Timer::Timestamp, Timer> mEvents;
    uint32_t mLastTimerId;
#ifndef WIN32
    int mTimerFd;
    IOEvent* mTimerIOEvent;
#endif
};