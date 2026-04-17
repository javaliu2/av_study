#include "live/MediaSource.h"
#include "base/Logger.h"
/**
 * 双队列实现的价值：
 * （1）避免频繁malloc/free, 循环复用mFrames，体现了内存池思想
 * （2）解耦读文件（生产）和发网络（消费）
 * （3）支持缓冲
 */
MediaSource::MediaSource(UsageEnvironment* env) : mEnv(env), mFps(0) {
    for (int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        mFrameInputQueue.push(&mFrames[i]);
    }
    mTask.setTaskCallback(taskCallback, this);
}

MediaSource::~MediaSource() {
    LOG_INFO("~MediaSource()");
}

MediaFrame* MediaSource::getFrameFromOutputQueue() {
    std::lock_guard<std::mutex> lck(mMtx);
    if (mFrameOutputQueue.empty()) {
        return NULL;
    }
    MediaFrame* frame = mFrameOutputQueue.front();
    mFrameOutputQueue.pop();
    return frame;
}

// 将frame加入mFrameInputQueue队列，同时将mTask任务加入线程池以填充该frame
void MediaSource::putFrameToInputQueue(MediaFrame* frame) {
    std::lock_guard<std::mutex> lck(mMtx);
    mFrameInputQueue.push(frame);
    mEnv->threadPool()->addTask(mTask);
}

void MediaSource::taskCallback(void* arg) {
    MediaSource* source = (MediaSource*)arg;
    source->handleTask();
}