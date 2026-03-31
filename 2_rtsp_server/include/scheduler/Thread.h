#pragma once
#include <thread>
class Thread {
public:
    virtual ~Thread();

    bool start(void* arg);
    // G-bro说，慎用detach，因为会出现线程不可控的问题，会出现UB现象。所以不知道，作者在这里设计detach是考虑到什么场景
    bool detach();
    bool join();

protected:
    Thread();
    virtual void run(void* arg) = 0;

private:
    void threadRun();  // 修改API

private:
    void* mArg;
    bool mIsStart;
    bool mIsDetach;
    std::thread mThreadId;
};