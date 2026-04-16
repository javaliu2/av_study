#pragma once
#include "scheduler/UsageEnvironment.h"
#include "scheduler/Event.h"
#include "live/Buffer.h"

class TcpConnection {
public:
    typedef void (*DisConnectCallback)(void*, int);
    TcpConnection(UsageEnvironment* env, int clientFd);
    virtual ~TcpConnection();

    // 设置回调函数
    void setDisConnectCallback(DisConnectCallback cb, void* arg);

protected:
    void enableReadHandling();
    void enableWriteHandling();
    void enableErrorHandling();
    void disableReadHandling();
    void disableWriteHandling();
    void disableErrorHandling();

    // 回调函数
    void handleRead();  // 针对可读事件的数据在 连接层面 的操作
    virtual void handleReadBytes();  // 针对可读事件的数据在 业务层面 的操作
    virtual void handleWrite();
    virtual void handleError();

    void handleDisConnect();
private:
    // 设置回调函数
    static void readCallback(void* arg);
    static void writeCallback(void* arg);
    static void errorCallback(void* arg);

protected:
    UsageEnvironment* mEnv;
    int mClientFd;
    IOEvent* mClientIOEvent;
    DisConnectCallback mDisConnectCallback;
    void* mArg;
    // 由于数据是通过tcp进行收发，所以把数据层设置在Tcp类中是合理的，业务层是对数据的处理
    // tcp层是对数据的收发操作
    Buffer mInputBuffer;
    Buffer mOutBuffer;
    char mBuffer[2048];
};