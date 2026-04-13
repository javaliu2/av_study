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