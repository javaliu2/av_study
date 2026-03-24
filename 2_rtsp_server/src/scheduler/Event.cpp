#include "Event.h"
#include <stdint.h>
#include "Logger.h"
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