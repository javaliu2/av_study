#include "Event.h"
#include <stdint.h>
#include "Logger.h"

// ==== IOEvent ====
IOEvent* IOEvent::createNew(int fd, void* arg) {
    if (fd < 0) {
        return NULL;
    }
    return new IOEvent(fd, arg);
}

IOEvent* IOEvent::createNew(int fd) {
    if (fd < 0) {
        return NULL;
    }
    return new IOEvent(fd, NULL);
}

IOEvent::IOEvent(int fd, void* arg) : mFd(fd), mArg(arg), 
mEvent(EVENT_NONE), mREvent(EVENT_NONE), 
mReadCallback(NULL), mWriteCallback(NULL), mErrorCallback(NULL)
{
    LOG_INFO("IOEvent() fd=%d", mFd);
}

IOEvent::~IOEvent() {
    LOG_INFO("~IOEvent() fd=%d", mFd);
}

void IOEvent::handleEvent() {
    if (mReadCallback && (mREvent & EVENT_READ)) {
        mReadCallback(mArg);
    }
    if (mWriteCallback && (mREvent & EVENT_WRITE)) {
        mWriteCallback(mArg);
    }
    if (mErrorCallback && (mREvent & EVENT_ERROR)) {
        mErrorCallback(mArg);
    }
}

// ==== TimerEvent ====
TimerEvent* TimerEvent::createNew(void* arg) {
    return new TimerEvent(arg);
}

TimerEvent* TimerEvent::createNew() {
    return new TimerEvent(NULL);
}

TimerEvent::TimerEvent(void* arg) : 
    mArg(arg), mTimeoutCallback(NULL), mIsStop(false) {
        LOG_INFO("TimerEvent()");
}

TimerEvent::~TimerEvent() {
    LOG_INFO("~TimerEvent()");
}

bool TimerEvent::handleEvent() {
    // TODO mIsStop 是谁的停止状态。Timer的还是TimerEvent的？按照OOP原则来说，应该是TimerEvent的
    if (mIsStop) {
        return mIsStop;
    }
    if (mTimeoutCallback) {
        mTimeoutCallback(mArg);
    }
    return mIsStop;
}

void TimerEvent::stop() {
    mIsStop = true;
}

// ==== TriggerEvent ====
TriggerEvent* TriggerEvent::createNew(void* arg) {
    return new TriggerEvent(arg);
}

TriggerEvent* TriggerEvent::createNew() {
    return new TriggerEvent(NULL);
}

TriggerEvent::TriggerEvent(void* arg) : mArg(arg), mTriggerCallback(NULL) {
    LOG_INFO("TriggerEvent()");
}
TriggerEvent::~TriggerEvent() {
    LOG_INFO("~TriggerEvent()");
}

void TriggerEvent::handleEvent() {
    if(mTriggerCallback) {
        mTriggerCallback(mArg);
    }
}