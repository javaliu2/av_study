# 高性能RTSP服务器

## 1、cpp相关

### 1、虚函数和纯虚函数的区别
有纯虚函数的类为抽象类，不能实例化，提供一种接口规范。子类继承抽象类，要么实现所有纯虚函数，要么继续作为抽象类存在。
虚函数父类可以有实现，给子类提供功能和扩展，子类继承父类的实现或者重写父类的方法。

### 2、friend class是什么东西

友元类，声明的友元类可以访问其所在类的所有成员，不受访问权限的限制

```c++
class Timer {
private:
    friend class TimerManager;  // 声明TimerManager为Timer的友元类
}
```

**补充：**

1、友元的本质

友元不是类的成员，而是**权限授权**，打破了 C++ 的封装性 ，仅在需要紧密协作的类之间使用。

2、声明位置不影响效果

友元声明写在private、protected、public区域效果完全一样。

### 3、时间单位

| 单位 | 符号 | 与秒的关系 | 与纳秒的关系 |
| ---- | ---- | ---------- | ------------ |
| 秒   | s    | 1          | 10⁹ ns       |
| 毫秒 | ms   | 10⁻³       | 10⁶ ns       |
| 微秒 | μs   | 10⁻⁶       | 10³ ns       |
| 纳秒 | ns   | 10⁻⁹       | 1            |

### 4、mutex和lock的关系
1、mutex是互斥量，作用是保证同一时间只有一个线程能访问某段代码或数据，是锁本体
2、lock不是具体类型，而是一个动作或状态（用来加锁解锁），是锁的管理器

### 5、lock_guard和unique_lock本质区别？
1、一句话核心
lock_guard = 极简自动锁（只能上锁/解锁）；
unique_lock = 功能完整的智能锁（可控、可转移、可配合条件变量）
2、直观对比
| 特性                    | lock_guard | unique_lock |
| --------------------- | ---------- | ----------- |
| 自动加锁                  | ✅          | ✅           |
| 自动解锁                  | ✅          | ✅           |
| 手动 unlock             | ❌          | ✅           |
| 手动 lock               | ❌          | ✅           |
| 可延迟加锁                 | ❌          | ✅           |
| 可转移所有权                | ❌          | ✅           |
| 支持 condition_variable | ❌          | ✅           |
| 性能                    | 更轻量        | 稍重          |

## 2、业务相关
### 1、AAC中AudioSpecificConfig字段
在SDP中通常使用两个字节表示，第一个字节高5位是object type，低3位是采样率索引index的高3位；
第二个字节的高1位是index的低1位，第6到第3位是channel
👉 第1个字节（8 bits）
| b7 b6 b5 b4 b3 | b2 b1 b0 |
| object type    | freq idx 高3位 |
👉 第2个字节（8 bits）
| b7        | b6 b5 b4 b3 | b2 b1 b0 |
| freq低1位 | channel cfg | 备用 |

### 2、梳理定时器相关的操作
### 1. 类EventScheduler
构造的对象起名为gScheduler（g即global之意）

构造函数：构造成员变量mPoller（类型：Poller类）和mTimerManager（类型：TimerManager类），构造后者的时候将对象指针**this**传递给该对象
#### 1.1 类Poller
构造的对象起名为gPoller

构造函数：将mReadSet、mWriteSet、mExceptionSet均置为0
#### 1.2 类TimerManager
1、构造的对象起名为gTimerManager

2、构造函数：由于scheduler构造时传递了**this**指针，即gScheduler对象，使用该对象获取到gPoller对象，完成本类成员变量mPoller的赋值，同时设置gScheduler的函数指针变量mTimerManagerReadCallback的值为本类的readCallback函数

3、readCallback函数

### 2. 定时器事件的加入
#### 1. 类TimeEvent
包括成员变量mArg和mTimeoutCallback，其handleEvent函数实现如下：
```c
void TimerEvent::handleEvent() {
    if (mTimeoutCallback) {
        mTimeoutCallback(mArg);
    }
}
```

#### 2. 类Sink
以H264FileSink为例说明

1、构造的对象起名为h264FileSink

2、构造函数：构造TimerEvent对象h264SinkTimerEvent，将**this**传递给该对象(完成其mArg参数的赋值)，同时设置h264SinkTimerEvent的mTimeoutCallback函数指针变量的值为本类的静态cbTimeout函数，该函数实现如下：
```c
void Sink::cbTimeout(void* arg) {
    Sink* sink = (Sink*)arg;
    sink->handleTimeout();
}
```

由于传递了**this**对象，因此调用h264SinkTimerEvent的handleEvent函数，调用的是h264FileSink对象的handleTimeout函数（cbTimeout函数的处理实现了这一目的）

3、runEvery函数的实现如下，其中```mEnv->scheduler()```获取到的就是gScheduler对象，addTimerEventRunEvery函数调用的是gTimerManager对象的addTimer函数。
```c
void Sink::runEvery(int interval) {
  mTimerId = mEnv->scheduler()->addTimerEventRunEvery(mTimerEvent, interval);
}
```

### 3、梳理session相关
#### 3.1、MediaSessionManager类
维护一个map，key为session名字，value为MediaSession*。

提供关于增、删、获取指定session名字所对应MediaSession对象的方法
#### 3.2、MediaSession类
提供会话，一个会话包含MEDIA_MAX_TRACK_NUM个Track对象，Track类定义在MediaSession类中，其声明如下：
```c
class Track {
    public:
        Sink* mSink;
        int mTrackId;
        bool mIsAlive;
        std::list<RtpInstance*> mRtpInstances;
    };
```
Track即媒体通道，是音频通道还是视频通道，MEDIA_MAX_TRACK_NUM定义为2，即一个MediaSession对象最多提供2个媒体通道。

### 4、梳理source和sink之间的关系
#### 4.1、思想
将source注册到sink中，两者通过倒腾mFrameInputQueue和mFrameOutputQueue队列实现媒体文件数据资源的生产和消费。

#### 4.2、source对象的创建
创建source对象时，首先调用父类的构造函数，将DEFAULT_FRAME_NUM个MediaFrame对象（预先分配的固定的内存资源，便于重用）加入mFrameInputQueue队列，同时设置task回调函数，主要逻辑如下：
```c
void FileMediaSource::handleTask() {
    std::lock_guard<std::mutex> lck(mMtx);
    if (mFrameInputQueue.empty()) {
        return;
    }
    MediaFrame* frame = mFrameInputQueue.front();
    frame->mSize = getFrameFromFile(frame->temp, FRAME_MAX_SIZE);
    if (frame->mSize < 0) {
        return;
    }
    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}
```
其次，执行子类的构造函数，打开h264或者aac文件获得文件流，向线程池中加入DEFAULT_FRAME_NUM个task。task回调函数是从mFrameInputQueue pop一个帧，填充该帧文件数据，将填充好的帧数据push到mFrameOutputQueue。所以这里的input和output是针对server来说的，input就是输入到server的数据，output就是要离开server的数据（发送到网络上）。

#### 4.3、sink对象的创建
创建sink对象时，父类构造函数创建timer事件，然后设置其回调函数handleTimeout。实现如下：
```c
void Sink::handleTimeout() {
    MediaFrame* frame = mMediaSource->getFrameFromOutputQueue();
    if (!frame) {
        return;
    }
    this->sendFrame(frame);
    mMediaSource->putFrameToInputQueue(frame);
}
```
由于sink持有source的引用，因此可以操作source的两个帧队列，sendFrame函数的话，aac和h264有各自的处理。

子类的构造函数通过独立的线程启动一定间隔的timer事件监听。

### 5、梳理rtsp server
#### 5.1、RtspSever的创建
创建RtspServer对象，注册accept事件监听，处理函数是创建RtspConnection对象
#### 5.2、RtspConnection的创建
当有client连接到达，会得到client_fd，执行以上处理函数去创建RtspConnection对象。首先调用父类TcpConnection的构造函数，该构造注册了client_fd的可读事件监听，其处理函数为解析rtsp请求并做出对应处理。

针对SETUP请求，由于RtspSever对象持有创建MediaSessionManager对象，因此尝试获取SETUP解析到的sessonName所对应的session对象，如果成功，那么创建对应track上的RtpInstance对象。

#### 5.3、补充
Buffer发送h264和aac文件的时候都没有使用，只是接受rtsp请求和发送rtsp响应的时候用到了。

### 6、日志分析
如下是日志输出，其中tid为15729502191765196471的是main线程，tid为18137369640724998020是处理定时器事件的线程。

本例中，h264文件的fps为30，那么他的定时器事件时间间隔为1000/30~=33；aac文件的fps为43，他的定时器事件时间间隔为1000/43~=23。

```c
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\RtspServer.cpp:16 RtspServer][tid=15729502191765196471] rtsp://127.0.0.1:8554 fd=244
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Event.cpp:23 IOEvent][tid=15729502191765196471] IOEvent() fd=244
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Event.cpp:85 TriggerEvent][tid=15729502191765196471] TriggerEvent()
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\main.cpp:22 main][tid=15729502191765196471] ------------ session init start ------------
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\MediaSession.cpp:17 MediaSession][tid=15729502191765196471] MediaSession(), sessionName=test
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\Sink.cpp:10 Sink][tid=15729502191765196471] Sink()
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Event.cpp:53 TimerEvent][tid=15729502191765196471] TimerEvent()
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\H264FileSink.cpp:17 H264FileSink][tid=15729502191765196471] H264FileSink()
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\EventScheduler.cpp:90 addTimerEventRunEvery][tid=15729502191765196471] timestamp=9608266433  
// 将该时刻9608266433触发的定时器(h264 file sink)加入timerManager
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\Sink.cpp:10 Sink][tid=15729502191765196471] Sink()
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Event.cpp:53 TimerEvent][tid=15729502191765196471] TimerEvent()
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\AACFileSink.cpp:16 AACFileSink][tid=15729502191765196471] AACFileSink()
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\EventScheduler.cpp:90 addTimerEventRunEvery][tid=15729502191765196471]  timestamp=9608266436  
// 将该时刻9608266436触发的定时器(aac file sink)加入timerManager
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\main.cpp:36 main][tid=15729502191765196471] ------------ session init end ------------
[INFO ][C:\Users\23590\Documents\av_study\2_rtsp_server\src\live\RtspServer.cpp:27 start][tid=15729502191765196471] RtspServer::start()
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Timer.cpp:164 handleRead][tid=18137369640724998020] timestamp=9608266433
// 处理9608266433时刻触发的定时器事件，即h264 file sink事件，由于是循环的定时器事件，故将9608266433+33=9608266466时刻触发的定时器加入timerManager
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Timer.cpp:164 handleRead][tid=18137369640724998020] timestamp=9608266437
// // 处理9608266437时刻触发的定时器事件，即aac file sink事件，由于是循环的定时器事件，故将9608266437+23=9608266460时刻触发的定时器加入timerManager
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Timer.cpp:164 handleRead][tid=18137369640724998020] timestamp=9608266460
[DEBUG][C:\Users\23590\Documents\av_study\2_rtsp_server\src\scheduler\Timer.cpp:164 handleRead][tid=18137369640724998020] timestamp=9608266466
```

## 3、库函数
fcntl中F_SETFL和F_SETFD的区别？
| 对比             | F_SETFL           | F_SETFD      |
| -------------- | ----------------- | ------------ |
| 作用对象           | 文件状态（file status） | 文件描述符（fd 本身） |
| 是否共享（dup/fork） | ✅ 共享              | ❌ 不共享        |
| 常见 flag        | O_NONBLOCK        | FD_CLOEXEC   |
| 影响范围           | I/O 行为            | 进程 exec 行为   |

## 4、Linux相关
每个socket本质是一个内核结构（简化理解）：
```c
struct socket {
    本地IP
    本地端口
    对端IP
    对端端口
    发送缓冲区
    接收缓冲区
    状态（ESTABLISHED / LISTEN）
}
```
**listenfd:**
```c
状态：LISTEN
没有对端
```
**connfd:**
```c
状态：ESTABLISHED
有完整四元组
```


