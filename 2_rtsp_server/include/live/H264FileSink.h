#pragma once
#include "stdint.h"
#include "Sink.h"

class H264FileSink : public Sink {
public:
    static H264FileSink* createNew(UsageEnvironment* env, MediaSource* mediaSource);

    H264FileSink(UsageEnvironment* env, MediaSource* mediaSource);
    ~H264FileSink() override;
    std::string getMediaDescription(uint16_t port) override;
    std::string getAttribute() override;
    void sendFrame(MediaFrame* frame) override;

private:
    RtpPacket mRtpPacket;
    int mClockRate;
    int mFps;
};