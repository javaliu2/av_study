#pragma once
#include "Poller.h"
#include <vector>
#ifndef WIN32
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <uinstd.h>
#else
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>
#endif

class SelectPoller : public Poller {
public:
    SelectPoller();
    virtual ~SelectPoller();

    static SelectPoller* createNew();

    virtual bool addIOEvent(IOEvent* event);
    virtual bool updateIOEvent(IOEvent* event);
    virtual bool removeIOEvent(IOEvent* event);
    virtual void handleEvent();

private:
    fd_set mReadSet;
    fd_set mWriteSet;
    fd_set mExceptionSet;
    int mMaxFd;  // select调用第一个参数nfds
    std::vector<IOEvent*> mIOEvents;  // 存储临时活跃的IO事件对象
};