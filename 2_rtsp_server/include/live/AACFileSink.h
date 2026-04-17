#pragma once
#include "scheduler/UsageEnvironment.h"
#include "live/Sink.h"
#include "MediaSource.h"

class AACFileSink : public Sink {
public:
    static AACFileSink* createNew(UsageEnvironment* env, MediaSource* mediaSource);

    AACFileSink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType);
    ~AACFileSink() override;

    std::string getMediaDescription(uint16_t port) override;
    std::string getAttribute() override;

protected:
    void sendFrame(MediaFrame* frame) override;

private:
    RtpPacket mRtpPacket;
    uint32_t mSampleRate;  // 采样频率
    uint32_t mChannels;  // 通道数
    int mFps;
};