#include "SelectPoller.h"
#include "Logger.h"

SelectPoller::SelectPoller() {
    FD_ZERO(&mReadSet);
    FD_ZERO(&mWriteSet);
    FD_ZERO(&mExceptionSet);
}

SelectPoller::~SelectPoller() {

}

SelectPoller* SelectPoller::createNew() {
    return new SelectPoller();
}

bool SelectPoller::addIOEvent(IOEvent* event) {
    return updateIOEvent(event);
}

bool SelectPoller::updateIOEvent(IOEvent* event) {
    int fd = event->getFd();
    if (fd < 0) {
        LOG_ERROR("fd=%d", fd);
        return false;
    }
    // 将fd上监听的读、写、错误事件全部清空，防止污染新事件的设置
    // 举例：原来fd上有‘读’事件监听，新的事件监听是‘写’和‘错误’，如果不清空，那么fd上就有三个事件监听了，而‘读’不是我们想要的
    FD_CLR(fd, &mReadSet);  // clear: 将set中监听的fd删除
    FD_CLR(fd, &mWriteSet);
    FD_CLR(fd, &mExceptionSet);

    IOEventMap::iterator it = mEventMap.find(fd);
    if (it != mEventMap.end()) {  // 已经有该fd，那么修改
        if (event->isReadHandling()) {
            FD_SET(fd, &mReadSet);
        }
        if (event->isWriteHandling()) {
            FD_SET(fd, &mWriteSet);
        }
        if (event->isErrorHandling()) {
            FD_SET(fd, &mExceptionSet);
        }
    } else {  // 添加IO事件
        if (event->isReadHandling()) {
            FD_SET(fd, &mReadSet);
        }
        if (event->isWriteHandling()) {
            FD_SET(fd, &mWriteSet);
        }
        if (event->isErrorHandling()) {
            FD_SET(fd, &mExceptionSet);
        }
        mEventMap.insert(std::make_pair(fd, event));
    }

    if (mEventMap.empty()) {
        mMaxFd = 0;
    } else {
        mMaxFd = mEventMap.rbegin()->first + 1;  // map会对key按照升序进行排序，所以最后一对pair的key就是最大的fd
        // +1是由于select系统调用中nfds为最大fd+1
    }
    return true;
}

bool SelectPoller::removeIOEvent(IOEvent* event) {
    int fd = event->getFd();
    if (fd < 0) {
        return false;
    }
    FD_CLR(fd, &mReadSet);
    FD_CLR(fd, &mWriteSet);
    FD_CLR(fd, &mExceptionSet);

    IOEventMap::iterator it = mEventMap.find(fd);
    if (it != mEventMap.end()) {
        mEventMap.erase(it);
    }
    if (mEventMap.empty()) {
        mMaxFd = 0;
    } else {
        mMaxFd = mEventMap.rbegin()->first + 1;
    }
    return true;
}

void SelectPoller::handleEvent() {
    fd_set readSet = mReadSet;
    fd_set writeSet = mWriteSet;
    fd_set exceptionSet = mExceptionSet;
    struct timeval timeout;
    int ret;
    int rEvent;

    timeout.tv_sec = 1000; // 秒
    timeout.tv_usec = 0;  // 微秒

    ret = select(mMaxFd, &readSet, &writeSet, &exceptionSet, &timeout);
    if (ret < 0) {
        return;
    }
    // select返回后，readSet、writeSet、exceptionSet中只保留就绪事件的fd
    for (IOEventMap::iterator it = mEventMap.begin(); it != mEventMap.end(); ++it) {
        rEvent = 0;
        int fd = it->first;
        if (FD_ISSET(fd, &readSet)) {
            rEvent |= IOEvent::EVENT_READ;
        }
        if (FD_ISSET(fd, &writeSet)) {
            rEvent |= IOEvent::EVENT_WRITE;
        }
        if (FD_ISSET(fd, &exceptionSet)) {
            rEvent |= IOEvent::EVENT_ERROR;
        }
        if (rEvent != 0) {
            it->second->setREvent(rEvent);
            mIOEvents.push_back(it->second);
        }
    }
    for (auto& ioEvent : mIOEvents) {
        ioEvent->handleEvent();
    }
    mIOEvents.clear();
}