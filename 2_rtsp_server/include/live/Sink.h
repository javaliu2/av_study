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
    uint8_t mSeq;
    uint8_t mTimestamp;
    uint8_t mSSRC;

private:
    TimerEvent* mTimerEvent;
    Timer::TimerId mTimerId;
};