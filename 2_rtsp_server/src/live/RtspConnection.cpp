#include "RtspConnection.h"
#include "RtspServer.h"
#include <stdio.h>
#include <string.h>
#include "Rtp.h"
#include "MediaSession.h"
#include "MediaSessionManager.h"
#include "Logger.h"
#include "Version.h"
/**
 * 该cpp文件作用域内的辅助函数，G-bro推荐使用namespace包裹变量和函数
 * 注意，他不是类函数，类函数在cpp实现的时候，必须不能加static，否则发生语义冲突
 */
static void getPeerIp(int fd, std::string& ip) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    getpeername(fd, (sockaddr*)&addr, &addrlen);
    ip = inet_ntoa(addr.sin_addr);
}

RtspConnection* RtspConnection::createNew(RtspServer* rtspServer, int clientFd) {
    return new RtspConnection(rtspServer, clientFd);
}

RtspConnection::RtspConnection(RtspServer* rtspServer, int clientFd) : 
    TcpConnection(rtspServer->env(), clientFd),
    mRtspServer(rtspServer), 
    mMethod(RtspConnection::Method::NONE),
    mTrackId(MediaSession::TrackId::TrackIdNone),
    mSessionId(rand()),  // 随机数作为session id
    mIsRtpOverTcp(false), mStreamPrefix("track") {
    
    LOG_INFO("RtspConnection() mClientFd=%d", mClientFd);
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        mRtpInstances[i] = nullptr;
        mRtcpInstances[i] = nullptr;
    }
    getPeerIp(clientFd, mPeerIp);  // 辅助函数，所有对象具有相同的操作
}

RtspConnection::~RtspConnection() {
    LOG_INFO("~RtspConnection() mClientFd=%d", mClientFd);
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mRtpInstances[i]) {
            MediaSession* session = mRtspServer->mSessMgr->getSession(mSuffix);
            if (!session) {
                session->removeRtpInstance(mRtpInstances[i]);
            }
            delete mRtpInstances[i];
        }
        if (mRtcpInstances[i]) {
            delete mRtcpInstances[i];
        }
    }
}