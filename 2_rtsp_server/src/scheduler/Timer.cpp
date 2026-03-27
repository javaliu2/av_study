#include "Timer.h"
#ifndef WIN32
#include <sys/timerfd.h>
#endif
#include <timer.h>
#include <chrono>
#include "Event.h"
#include "EventScheduler.h"
#include "Poller.h"
#include "Logger.h"

static bool timerFdSetTime(int fd, Timer::Timestamp when, Timer::TimeInterval period) {
#ifndef WIN32
    struct itimerspec newVal;
    // it_value表示定时器第一次什么时候触发
    newVal.it_value.tv_sec = when / 1000;  // ms -> s
    newVal.it_value.tv_nsec = when % 1000 * 1000 * 1000;  // ms -> ns(1ms = 10^6ns)
    // it_interval表示定时器的触发间隔
    newVal.it_interval.tv_sec = period / 1000;
    newVal.it_interval.tv_nsec = period % 1000 * 1000 * 1000;

    int oldValue = timerfd_settime(fd, TFD_TIMER_ABSTIME, &newVal, NULL);  // 使用了TFD_TIMER_ABSTIME所以这里的when是绝对时间
    if (oldValue < 0) {
        return false;
    }
    return true;
#endif
    return true;
}

Timer::Timer(TimerEvent* event, Timestamp timestamp, TimeInterval timeInterval, TimerId timerId) :
mTimerEvent(event), mTimestamp(timestamp), mTimeInterval(timeInterval), mTimerId(timerId) {
    if (timeInterval > 0) {
        mRepeat = true;  // 循环定时器
    } else {
        mRepeat = false;  // 一次性定时器
    }
}

Timer::~Timer() {

}

// 获取系统从启动到现在的毫秒数
Timer::Timestamp Timer::getCurTime() {
#ifndef WIN32
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1000 + now.tv_nsec / 1000000);  // ms
#else
    long long now = std::chrono::steady_clock::now().time_since_epoch().count();
    return now / 1000000;
#endif
}

Timer::Timestamp Timer::getCurTimestamp() {
    // time_since_epoch获取时间差，count将时间差提取为数字
    return std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::system_clock::now().time_since_epoch()).count();
}

bool Timer::handleEvent() {
    if (!mTimerEvent) {
        return false;
    }
    return mTimerEvent->handleEvent();
}

// =====

TimerManager* TimerManager::createNew(EventScheduler* scheduler) {
    if (!scheduler) {
        return NULL;
    }
    return new TimerManager(scheduler);
}

TimerManager::TimerManager(EventScheduler* scheduler) : 
    mPoller(scheduler->poller()),
    mLastTimerId(0) {
#ifndef WIN32
    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (mTimerFd < 0) {
        LOG_ERROR("create TimerFd error");
        return;
    } else {
        LOG_INFO("fd=%d", mTimerFd);
    }
    mTimerIOEvent = IOEvent::createNew(mTimerFd, this);
    mTimerIOEvent->setReadCallback(readCallback);
    mTimerIOEvent->enableReadHandling();
    modifyTimeout();
    mPoller->addIOEvent(mTimerIOEvent);
#else
    scheduler->setTimerManagerReadCallback(readCallback, this);
#endif
}

TimerManager::~TimerManager() {
#ifndef WIN32
    mPoller->removeIOEvent(mTimerIOEvent);
    delete mTimerIOEvent;
#endif
}

Timer::TimerId TimerManager::addTimer
    ( TimerEvent* event, Timer::Timestamp timestamp,
      Timer::TimeInterval timeInterval ) {
    ++mLastTimerId;  // 定时器ID递增
    Timer timer(event, timestamp, timeInterval, mLastTimerId);  // 构造一个定时器对象
    // TODO 存疑。这里加入的是值（对象的拷贝），而不是指针
    mTimers.insert(std::make_pair(mLastTimerId, timer));
    mEvents.insert(std::make_pair(timestamp, timer));  // 同一个时刻可能有多个定时器启动
    modifyTimeout();
    
    return mLastTimerId;
}

bool TimerManager::removeTimer(Timer::TimerId timerId) {
    std::map<Timer::TimerId, Timer>::iterator it = mTimers.find(timerId);
    if (it != mTimers.end()) {
        mTimers.erase(timerId);
        // TODO 还需要删除mEvents的事件
    }
    modifyTimeout();
    return true;
}

void TimerManager::modifyTimeout() {
#ifndef WIN32
    std::multimap<Timer::Timestamp, Timer>::iterator it = mEvents.begin();
    if (it != mEvents.end()) {
        Timer timer = it->second;
        timerFdSetTime(mTimerFd, timer.mTimestamp, time.mTimeInterval);
    } else {
        timerFdSetTime(mTimerFd, 0, 0);
    }
#endif
}

void TimerManager::readCallback(void* arg) {
    TimerManager* timerManager = (TimerManager*)arg;
    timerManager->handleRead();
}

void TimerManager::handleRead() {
    Timer::Timestamp timestamp = Timer::getCurTime();
    if (!mTimers.empty() && !mEvents.empty()) {
        auto it = mEvents.begin();
        Timer timer = it->second;
        int expire = timer.mTimestamp - timestamp;  // TimerManager被声明为Timer的友元类，因此其可以直接访问timer的私有属性
        // auto i = new Timer(NULL, 1, 1, 1);  // 调用私有的构造函数
        // delete i;
        if (timestamp > timer.mTimestamp || expire == 0) {
            bool timerEventIsStop = timer.handleEvent();
            mEvents.erase(it);
            if (timer.mRepeat) {
                if (timerEventIsStop) {
                    mTimers.erase(timer.mTimerId);
                } else {
                    timer.mTimestamp = timestamp + timer.mTimeInterval;
                    mEvents.insert(std::make_pair(timer.mTimestamp, timer));
                }
            } else {
                mTimers.erase(timer.mTimerId);
            }
        }
    }
    modifyTimeout();
}