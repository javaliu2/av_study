#pragma once
#include <string>
#include <stdint.h>

#include "Rtp.h"
#include "live/MediaSource.h"
#include "scheduler/Event.h"
#include "scheduler/UsageEnvironment.h"

class Sink {
public:
    enum PacketType {
        UNKNOW = -1,
        RTPPACKET = 0
    };
    typedef void (*SessionSendPacketCallback)(void* arg1, void* arg2, void* packet, PacketType packetType);
    Sink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType);
    virtual ~Sink();

    void stopTimerEvent();

    virtual std::string getMediaDescription(uint16_t port) = 0;
    virtual std::string getAttribute() = 0;

    void setSessionCb(SessionSendPacketCallback cb, void* arg1, void* arg2);

protected:
    virtual void sendFrame(MediaFrame* frame) = 0;
    void sendRtpPacket(RtpPacket* packet);

    void runEvery(int interval);

private:
    static void cbTimeout(void* arg);
    void handleTimeout();

protected:
    UsageEnvironment* mEnv;
    MediaSource* mMediaSource;
    SessionSendPacketCallback mSessionSendPacketCb;
    void* mArg1;
    void* mArg2;

    uint8_t mCsrcLen;
    uint8_t mExtension;
    uint8_t mPadding;
    uint8_t mVersion;
    uint8_t mPayloadType;
    uint8_t mMarker;
    // 还得是AI，人脑比不了，还得是大模型
    // 错误是之前以下三个字段粗心写成了uint8_t类型的
    uint16_t mSeq;
    uint32_t mTimestamp;
    uint32_t mSSRC;

private:
    TimerEvent* mTimerEvent;
    Timer::TimerId mTimerId;
};