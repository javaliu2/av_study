#include <string.h>
#include "live/AACFileMediaSource.h"
#include "base/Logger.h"

AACFileMediaSource* AACFileMediaSource::createNew(UsageEnvironment* env, const std::string& file) {
    return new AACFileMediaSource(env, file);
}

AACFileMediaSource::AACFileMediaSource(UsageEnvironment* env, const std::string& file) : MediaSource(env) {  // 调用父类的构造函数
    mSourceName = file;
    mFile = fopen(file.c_str(), "rb");
    setFps(43);  // 44100(1秒钟的采样点) / 1024(1帧的采样点) => 1秒43帧
    for (int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        // Q: 为什么要连续加这几个任务
        // A: mTask任务在父类中完成初始化，执行的是每一个子类的handleTask函数，连续加四个task是为了加快数据的读取吧
        // 不是，handleTask是线程互斥的，所以这里加入DEFAULT_FRAME_NUM个任务就是为了消耗掉mFrameInputQueue中的帧，获取这么多帧数据
        mEnv->threadPool()->addTask(mTask);  
    }
}

AACFileMediaSource::~AACFileMediaSource() {
    fclose(mFile);  // 对持有的文件资源进行关闭操作
}

void AACFileMediaSource::handleTask() {
    std::lock_guard<std::mutex> lck(mMtx);
    if (mFrameInputQueue.empty()) {
        return;
    }
    MediaFrame* frame = mFrameInputQueue.front();
    frame->mSize = getFrameFromAACFile(frame->temp, FRAME_MAX_SIZE);
    if (frame->mSize < 0) {
        return;
    }
    frame->mBuf = frame->temp;
    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}

bool AACFileMediaSource::parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
    memset(res, 0, sizeof(*res));

    if ((in[0] == 0xFF)&&((in[1] & 0xF0) == 0xF0)) {
        res->id = ((unsigned int) in[1] & 0x08) >> 3;
        res->layer = ((unsigned int) in[1] & 0x06) >> 1;
        res->protectionAbsent = (unsigned int) in[1] & 0x01;
        res->profile = ((unsigned int) in[2] & 0xc0) >> 6;
        res->samplingFreqIndex = ((unsigned int) in[2] & 0x3c) >> 2;
        res->privateBit = ((unsigned int) in[2] & 0x02) >> 1;
        res->channelCfg = ((((unsigned int) in[2] & 0x01) << 2) | (((unsigned int) in[3] & 0xc0) >> 6));
        res->originalCopy = ((unsigned int) in[3] & 0x20) >> 5;
        res->home = ((unsigned int) in[3] & 0x10) >> 4;
        res->copyrightIdentificationBit = ((unsigned int) in[3] & 0x08) >> 3;
        res->copyrightIdentificationStart = (unsigned int) in[3] & 0x04 >> 2;
        res->aacFrameLength = (((((unsigned int) in[3]) & 0x03) << 11) |
                                (((unsigned int)in[4] & 0xFF) << 3) |
                                    ((unsigned int)in[5] & 0xE0) >> 5) ;
        res->adtsBufferFullness = (((unsigned int) in[5] & 0x1f) << 6 |
                                        ((unsigned int) in[6] & 0xfc) >> 2);
        res->numberOfRawDataBlockInFrame = ((unsigned int) in[6] & 0x03);

        return true;
    } else {
        LOG_ERROR("failed to parse adts header");
        return false;
    }
}

int AACFileMediaSource::getFrameFromAACFile(uint8_t* buf, int size) {
    if (!mFile) {
        return -1;
    }
    uint8_t tmpBuf[7];  // adts headr 大小 7 bytes
    int ret;
    ret = fread(tmpBuf, 1, 7, mFile);
    if (ret <= 0) {
        fseek(mFile, 0, SEEK_SET);  // 将文件读取位置重置到文件最开头，（偏移位置：文件开头，偏移量：0）
        /**
         * SEEK_CUR: 从当前文件指针位置开始偏移
         * SEEK_END: 从文件末尾开始偏移
         * SEEK_SET: 从文件头开始偏移
         */
        ret = fread(tmpBuf, 1, 7, mFile);
        if (ret <= 0) {
            return -1;
        }
    }

    if (!parseAdtsHeader(tmpBuf, &mAdtsHeader)) {
        return -1;
    }

    if (mAdtsHeader.aacFrameLength > size) {
        return -1;
    }

    memcpy(buf, tmpBuf, 7);
    ret = fread(buf+7, 1, mAdtsHeader.aacFrameLength-7, mFile);
    if ( ret < 0 ) {
        LOG_ERROR("read error");
        return -1;
    }
    return mAdtsHeader.aacFrameLength;
}
