#include "Sink.h"
#include "SocketsOps.h"
#include "Logger.h"

Sink::Sink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType) : 
    mMediaSource(mediaSource), mEnv(env), mCsrcLen(0), mExtension(0),
    mPadding(0), mVersion(RTP_VERSION), mPayloadType(payloadType), mMarker(0),
    mSeq(0), mSSRC(rand()), mTimestamp(0), mTimerId(0), mSessionSendPacketCb(nullptr),
    mArg1(nullptr), mArg2(nullptr) {
    LOG_INFO("Sink()");
    mTimerEvent = TimerEvent::createNew(this);
    mTimerEvent->setTimeoutCallback(cbTimeout);
}

Sink::~Sink() {
    LOG_INFO("~Sink()");
    delete mTimerEvent;
    delete mMediaSource;
}

void Sink::stopTimerEvent() {
    mTimerEvent->stop();
}
/**
 * @param cb 被回调函数
 * @param arg1 对象指针
 * @param arg2 被回调track对象指针
 */
void Sink::setSessionCb(SessionSendPacketCallback cb, void* arg1, void* arg2) {
    mSessionSendPacketCb = cb;
    mArg1 = arg1;
    mArg2 = arg2;
}

void Sink::sendRtpPacket(RtpPacket* packet) {
    RtpHeader* rtpHeader = packet->mRtpHeader;
    rtpHeader->csrcLen = mCsrcLen;
    rtpHeader->extension = mExtension;
    rtpHeader->padding = mPadding;
    rtpHeader->version = mVersion;
    rtpHeader->payloadType = mPayloadType;
    rtpHeader->marker = mMarker;
    rtpHeader->seq = htons(mSeq);
    rtpHeader->timestamp = htonl(mTimestamp);
    rtpHeader->ssrc = htonl(mSSRC);

    if (mSessionSendPacketCb) {
        mSessionSendPacketCb(mArg1, mArg2, packet, PacketType::RTPPACKET);
    }
}

void Sink::cbTimeout(void* arg) {
    Sink* sink = (Sink*)arg;
    sink->handleTimeout();
}

void Sink::handleTimeout() {
    MediaFrame* frame = mMediaSource->getFrameFromOutputQueue();
    if (!frame) {
        return;
    }
    this->sendFrame(frame);  // 由具体子类实现发送逻辑
    // 将使用过的frame插入输入队列，插入输入队列以后，加入一个子线程task,
    // 从文件中读取数据再次将输入写入到frame
    mMediaSource->putFrameToInputQueue(frame);
}

void Sink::runEvery(int interval) {
    mTimerId = mEnv->scheduler()->addTimerEventRunEvery(mTimerEvent, interval);
}