#include "MediaSession.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <algorithm>
#include <assert.h>
#include "Logger.h"

MediaSession* MediaSession::createNew(std::string sessionName) {
    return new MediaSession(sessionName);
}

// Q: 不懂这个multicast是啥
// A：组播
MediaSession::MediaSession(const std::string& sessionName) :
    mSessionName(sessionName), mIsStartMulticast(false) {
    LOG_INFO("MediaSession(), sessionName=%s", sessionName.data());
    mTracks[0].mTrackId = TrackId0;
    mTracks[0].mIsAlive = false;
    mTracks[1].mTrackId = TrackId1;
    mTracks[1].mIsAlive = false;

    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        // Rtp和Rtcp实例只负责协议层逻辑处理，不负责数据的传输
        mMulticastRtpInstances[i] = nullptr;
        mMulticastRtcpInstances[i] = nullptr;
    }
}

MediaSession::~MediaSession() {
    LOG_INFO("~MediaSession()");
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mMulticastRtpInstances[i]) {
            this->removeRtpInstance(mMulticastRtpInstances[i]);
            delete mMulticastRtpInstances[i];
        }
        if (mMulticastRtcpInstances[i]) {
            delete mMulticastRtcpInstances[i];
        }
    }
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mTracks[i].mIsAlive) {
            Sink* sink = mTracks[i].mSink;
            delete sink;
        }
    }
}

bool MediaSession::removeRtpInstance(RtpInstance* rtpInstance) {
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mTracks[i].mIsAlive == false) {
            continue;
        }
        std::list<RtpInstance*> item = mTracks[i].mRtpInstances;
        std::list<RtpInstance*>::iterator it = std::find(item.begin(), item.end(), rtpInstance);
        if (it == item.end()) {
            continue;
        }
        item.erase(it);
        return true;
    }
    return false;
}

MediaSession::Track* MediaSession::getTrack(MediaSession::TrackId trackId) {
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mTracks[i].mTrackId == trackId) {
            return &mTracks[i];
        }
    }
    return nullptr;
}

bool MediaSession::addSink(MediaSession::TrackId trackId, Sink* sink) {
    Track* track = getTrack(trackId);
    if (!track) {
        return false;
    }
    track->mSink = sink;
    track->mIsAlive = true;
    sink->setSessionCb(MediaSession::sendPacketCallback, this, track);
    return true;
}

bool MediaSession::addRtpInstance(MediaSession::TrackId trackId, RtpInstance* rtpInstance) {
    Track* track = getTrack(trackId);
    if (!track || track->mIsAlive != true) {
        return false;
    }
    track->mRtpInstances.push_back(rtpInstance);
    return true;
}

void MediaSession::sendPacketCallback(void* arg1, void* arg2, void* packet, Sink::PacketType packetType) {
    RtpPacket* rtpPacket = (RtpPacket*)packet;
    MediaSession* session = (MediaSession*)arg1;
    MediaSession::Track* track = (MediaSession::Track*)arg2;

    session->handleSendRtpPacket(track, rtpPacket);
}

void MediaSession::handleSendRtpPacket(MediaSession::Track* track, RtpPacket* rtpPacket) {
    for (auto it = track->mRtpInstances.begin(); it != track->mRtpInstances.end(); ++it) {
        RtpInstance* rtpInstance = *it;
        if (rtpInstance->getAlive()) {
            rtpInstance->send(rtpPacket);
        }
    }
}

std::string MediaSession::generateSdpDescription() {
    if (!mSdp.empty()) {
        return mSdp;
    }
    std::string ip = "0.0.0.0";
    char buf[2048] = {0};
    // （1）会话级别描述
    snprintf(buf, sizeof(buf), 
        "v=0\r\n"  // 版本号
        "o=- 9%ld 1 IN IP4 %s\r\n"  // <username><session id><session version><network type><address type><unicast/multicast address>
        "t=0 0\r\n"  // t=<start-time><stop-time>, 两者均为0的话，表示持久会话
        "a=control:*\r\n"  // 表示整个会话的控制URL，*是通配符，表示这是一个聚合控制的会话
        "a=type:broadcast\r\n",  // 这是一个广播流
        (long)time(NULL), ip.c_str());
    if (isStartMulticast()) {
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
        "a=rtcp-unicast: reflection\r\n");  // rtcp使用unicast，并且由服务器反射回客户端
    }
    // （2）媒体级别描述
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        uint16_t port = 0;
        if (mTracks[i].mIsAlive != true) {
            continue;
        }
        if(isStartMulticast()) {
            port = getMulticastDestRtpPort((TrackId)i);  // type-cast
        }
        
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                "%s\r\n", mTracks[i].mSink->getMediaDescription(port).c_str());
        if (isStartMulticast()) {
             snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                    "c=IN IP4 %s/255\r\n", getMulticastDestAddr().c_str());
        } else {
            snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                    "c=IN IP4 0.0.0.0\r\n");
        }
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                "%s\r\n", mTracks[i].mSink->getAttribute().c_str());
        snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
                "a=control:track%d\r\n", mTracks[i].mTrackId);
    }
    mSdp = buf;
    return mSdp;
}

bool MediaSession::isStartMulticast() {
    return mIsStartMulticast;
}

uint16_t MediaSession::getMulticastDestRtpPort(TrackId trackId) {
    // 检查trackId是否合法，合法的话，检查rtp实例是否存在
    if (trackId <= TrackIdNone || trackId > TrackId1 || !mMulticastRtpInstances[trackId]) {
        return -1;
    }
    return mMulticastRtpInstances[trackId]->getPeerPort();
}

bool MediaSession::startMulticast() {
    // 随机生成组播地址
    struct sockaddr_in addr = {0};
    uint32_t range = 0xE8FFFFFF - 0XE8000100;
    addr.sin_addr.s_addr = htonl(0XE8000100 + (rand() % range));
    mMulticastAddr = inet_ntoa(addr.sin_addr);

    int rtpSockfd1, rtcpSockfd1;
    int rtpSockfd2, rtcpSockfd2;
    uint16_t rtpPort1, rtcpPort1;
    uint16_t rtpPort2, rtcpPort2;
    bool ret;

    rtpSockfd1 = sockets::createUdpSock();
    assert(rtpSockfd1 > 0);
    rtcpSockfd1 = sockets::createUdpSock();
    assert(rtcpSockfd1 > 0);

    rtpSockfd2 = sockets::createUdpSock();
    assert(rtpSockfd2 > 0);
    rtcpSockfd2 = sockets::createUdpSock();
    assert(rtcpSockfd2 > 0);

    uint16_t port = rand() % 0xfffe;
    if (port < 10000) {
        port += 10000;
    }
    rtpPort1 = port;
    rtcpPort1 = port+1;
    rtpPort2 = rtcpPort1+1;
    rtcpPort2 = rtpPort2+1;

    mMulticastRtpInstances[TrackId0] = RtpInstance::createNewOverUdp(rtpSockfd1, 0, mMulticastAddr, rtpPort1);
    mMulticastRtpInstances[TrackId1] = RtpInstance::createNewOverUdp(rtpSockfd2, 0, mMulticastAddr, rtpPort2);
    mMulticastRtcpInstances[TrackId0] = RtcpInstance::createNew(rtcpSockfd1, 0, mMulticastAddr, rtcpPort1);
    mMulticastRtcpInstances[TrackId1] = RtcpInstance::createNew(rtcpSockfd2, 0, mMulticastAddr, rtcpPort2);

    this->addRtpInstance(TrackId0, mMulticastRtpInstances[TrackId0]);
    this->addRtpInstance(TrackId1, mMulticastRtpInstances[TrackId1]);
    mMulticastRtpInstances[TrackId0]->setAlive(true);
    mMulticastRtpInstances[TrackId1]->setAlive(true);

    mIsStartMulticast = true;
    return true;
}