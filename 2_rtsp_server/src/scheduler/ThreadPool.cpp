#include "scheduler/ThreadPool.h"
#include "base/Logger.h"

ThreadPool* ThreadPool::createNew(int num) {
    return new ThreadPool(num);
}

// mThreads(num): 会调用 MThread 的构造函数，创建 num 个 MThread 对象
ThreadPool::ThreadPool(int num) : mThreads(num), mQuit(false) {
    createThreads();
}

ThreadPool::~ThreadPool() {
    cancelThreads();
}

void ThreadPool::createThreads() {
    // std::unique_lock <std::mutex> lck(mMtx);
    // 有没有共享数据被多个线程读写？
    // 无，所以使用锁没有必要。
    for (auto& mThread : mThreads) {
        mThread.start(this);
    }
}

void ThreadPool::cancelThreads() {
    // 不涉及到其他复杂的操作，故可以使用 lock_guard
    {
        std::lock_guard <std::mutex> lck(mMtx);  // 保证对共享变量的互斥访问
        mQuit = true; // 共享变量
    }
    mCon.notify_all();
    for (auto& mThread : mThreads) {
        mThread.join();
    }
    mThreads.clear();  // 会调用MThread的析构函数
}

void ThreadPool::MThread::run(void* arg) {
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->loop();
}
/**
 * 临界区代码块
 */
void ThreadPool::loop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lck(mMtx);
            // wait(lock, 条件)
            // 条件指的是当前线程“是否可以继续往下执行”的条件
            // 在这里，当 mQuit 为 true (语义: 线程池要退出) 或者 任务队列不为空 的话，该线程继续执行
            // 否则，线程释放锁，进入阻塞状态等待被唤醒（这两个操作是原子性的）
            mCon.wait(lck, [this] {
                return mQuit || !mTaskQueue.empty();
            });
            if (mQuit && mTaskQueue.empty()) {  // 不能立马退出，需要将任务队列task处理完
                break;
            }
            task = mTaskQueue.front();  // 需要重载 operator= 以确保正确赋值
            mTaskQueue.pop();
        }  // 在这里就释放锁，防止处理task时间长，影响其他线程消费任务队列中task
        task.handle();
    }
}

void ThreadPool::addTask(ThreadPool::Task& task) {
    {
        std::lock_guard <std::mutex> lck(mMtx);
        mTaskQueue.push(task);
    }
    mCon.notify_one();
}