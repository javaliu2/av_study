#pragma once

// 定义一种名为 EventCallback 的函数指针类型
// 该类型定义的变量可以指向的函数为“返回值为void，且形参为任意数据类型的变量”
typedef void (*EventCallback)(void*);
/**
 * TriggerEvent是由程序“主动触发”的事件
 */
class TriggerEvent {
public:
    static TriggerEvent* createNew(void* arg);
    static TriggerEvent* createNew();

    TriggerEvent(void* arg);
    ~TriggerEvent();

    void setArg(void* arg) {
        mArg = arg;
    }
    void setTriggerCallback(EventCallback cb) {
        mTriggerCallback = cb;
    }
    void handleEvent();
private:
    void* mArg;  // 变量名前的m表示member，即成员变量
    EventCallback mTriggerCallback;
};

class TimerEvent {
public:
    static TimerEvent* createNew(void* arg);
    static TimerEvent* createNew();

    TimerEvent(void* arg);
    ~TimerEvent();

    void setArg(void* arg) {
        mArg = arg;
    }
    void setTimeoutCallback(EventCallback cb) {
        mTimeoutCallback = cb;
    }
    bool handleEvent();

    void stop();
private:
    void* mArg;
    EventCallback mTimeoutCallback;
    bool mIsStop;
};

class IOEvent {
public:
    enum IOEventType {
        EVENT_NONE = 0,
        EVENT_READ = 1,
        EVENT_WRITE = 2,
        EVENT_ERROR = 4,
    };
    static IOEvent* createNew(int fd, void* arg);
    static IOEvent* createNew(int fd);

    IOEvent(int fd, void* arg);
    ~IOEvent();
    
    int getFd() const {
        return mFd;
    }
    int getEvent() const {
        return mEvent;
    }
    void setREvent(int event) {
        mREvent = event;
    }
    void setArg(void* arg) {
        mArg = arg;
    }
    void setReadCallback(EventCallback cb) {
        mReadCallback = cb;
    }
    void setWriteCallback(EventCallback cb) {
        mWriteCallback = cb;
    }
    void setErrorCallback(EventCallback cb) {
        mErrorCallback = cb;
    }

    void enableReadHandling() {
        mEvent |= EVENT_READ;
    }
    void enableWriteHandling() {
        mEvent |= EVENT_WRITE;
    }
    void enableErrorHandling() {
        mEvent |= EVENT_ERROR;
    }
    void disableReadHandling() {
        mEvent &= ~EVENT_READ;
    }
    void disableWriteHandling() {
        mEvent &= ~EVENT_WRITE;
    }
    void disableErrorHandling() {
        mEvent &= ~EVENT_ERROR;
    }

    bool isNoneHandling() const {
        return mEvent == EVENT_NONE;
    }
    bool isReadHandling() const {
        return (mEvent & EVENT_READ) != 0;
    }
    bool isWriteHandling() const {
        return (mEvent & EVENT_WRITE) != 0;
    }
    bool isErrorHandling() const {
        return (mEvent & EVENT_ERROR) != 0;
    }

    void handleEvent();
private:
    int mFd;
    void* mArg;
    int mEvent;
    int mREvent;  // R: Ready，已经就绪的事件
    EventCallback mReadCallback;
    EventCallback mWriteCallback;
    EventCallback mErrorCallback;
};