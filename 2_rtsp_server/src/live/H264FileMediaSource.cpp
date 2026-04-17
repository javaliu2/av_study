#include "live/H264FileMediaSource.h"
#include "base/Logger.h"
#include <fcntl.h>

static inline bool startCode3(uint8_t* buf);
static inline bool startCode4(uint8_t* buf);

H264FileMediaSource* H264FileMediaSource::createNew(UsageEnvironment* env, const std::string& file) {
    return new H264FileMediaSource(env, file);
}

H264FileMediaSource::H264FileMediaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env) {
    mSourceName = file;
    mFile = fopen(file.c_str(), "rb");
    setFps(30);

    for (int i = 0; i < DEFAULT_FRAME_NUM; ++i) {
        mEnv->threadPool()->addTask(mTask);
    }
}

H264FileMediaSource::~H264FileMediaSource() {
    fclose(mFile);
}

void H264FileMediaSource::handleTask() {
    std::lock_guard <std::mutex> lck(mMtx);

    if (mFrameInputQueue.empty()) {
        return;
    }

    MediaFrame* frame = mFrameInputQueue.front();
    int startCodeNum = 0;

    while (true) {
        frame->mSize = getFrameFromH264File(frame->temp, FRAME_MAX_SIZE);
        if (frame->mSize < 0) {
            return;
        }
        if (startCode3(frame->temp)) {
            startCodeNum = 3;
        } else {
            startCodeNum = 4;
        }
        frame->mBuf = frame->temp + startCodeNum;
        frame->mSize -= startCodeNum;

        uint8_t naluType = frame->mBuf[0] & 0x1F;
        if (naluType == 0x09) {  // AUD(Access Unit Delimiter): 用来标识一帧的开始；不包含实际视频数据
            continue;
        } else if (0x07 == naluType || 0x08 == naluType) {  // SPS(sequence parameter set) and PPS(picture parameter set)
            break;
        } else {
            break;
        }
    }
    mFrameInputQueue.pop();
    mFrameOutputQueue.push(frame);
}

static inline bool startCode3(uint8_t* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
        return true;
    }
    return false;
}

static inline bool startCode4(uint8_t* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {
        return true;
    }
    return false;
}

static uint8_t* findNextStartCode(uint8_t* buf, int len) {
    int i;
    if (len < 3) {
        return nullptr;
    }
    for (i = 0; i < len - 3; ++i) {
        if (startCode3(buf) || startCode4(buf)) {
            return buf;
        }
        ++buf;
    }
    if (startCode3(buf)) {
        return buf;
    }
    return nullptr;
}

int H264FileMediaSource::getFrameFromH264File(uint8_t* frame, int size) {
    if (!mFile) {
        return -1;
    }

    int r, frameSize;
    uint8_t* nextStartCode;

    r = fread(frame, 1, size, mFile);
    if (!startCode3(frame) && !startCode4(frame)) {
        fseek(mFile, 0, SEEK_SET);
        LOG_ERROR("Read %s error, neither startCode3 or startCode4", mSourceName.c_str());
        return -1;
    }

    nextStartCode = findNextStartCode(frame + 3, r - 3);
    if (!nextStartCode) {
        if (feof(mFile)) {
            // End of file, reset to beginning for looping
            fseek(mFile, 0, SEEK_SET);
            frameSize = r;
        } else {
            // Unexpected error, no start code found
            fseek(mFile, 0, SEEK_SET);
            LOG_ERROR("Read %s error, no nextStartCode, r = %d", mSourceName.c_str(), r);
            return -1;
        }
    } else {
        frameSize = (nextStartCode - frame);
        fseek(mFile, frameSize-r, SEEK_CUR);
    }
    return frameSize;
}