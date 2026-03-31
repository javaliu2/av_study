#include "Thread.h"

Thread::Thread() : mArg(nullptr), mIsStart(false), mIsDetach(false) {

}

Thread::~Thread() {

}

bool Thread::start(void* arg) {  // arg指向ThreadPool对象
    mArg = arg;
    // std::thread(&类名::成员函数, 对象, 参数...)
    mThreadId = std::thread(&Thread::threadRun, this);  // this指定调用当前对象的threadRun方法
    mIsStart = true;
    return true;
}

bool Thread::detach() {
    if (!mIsStart) {
        return false;
    }
    if (mIsDetach) {
        return true;
    }
    mThreadId.detach();  // 不知道这里会不会有 竞态问题的出现。好像不会，mIsDetach是线程对象的成员变量
    mIsDetach = true;
    return true;
}

bool Thread::join() {
    if (!mIsStart || mIsDetach) {
        return false;
    }
    mThreadId.join();
    return true;
}

void Thread::threadRun() {
    run(mArg);
}