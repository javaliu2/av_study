#include "live/RtspConnection.h"
#include "live/RtspServer.h"
#include <stdio.h>
#include <string.h>
#include "live/Rtp.h"
#include "live/MediaSession.h"
#include "live/MediaSessionManager.h"
#include "base/Logger.h"
#include "base/Version.h"
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
/* 解析method、url、version三个字段
 * @param begin 指向所解析字符串的起始位置，include
 * @param end   指向所解析字符串的终止位置，exclude
*/
bool RtspConnection::parseRequest1(const char* begin, const char* end) {
    std::string message(begin, end);
    char method[64] = {0};
    char url[512] = {0};
    char version[64] = {0};

    if (sscanf(message.c_str(), "%s %s %s", method, url, version) != 3) {
        return false;
    }

    if (!strcmp(method, "OPTIONS")) {
        mMethod = OPTIONS;
    } else if (!strcmp(method, "DESCRIBE")) {
        mMethod = DESCRIBE;
    } else if (!strcmp(method, "SETUP")) {
        mMethod = SETUP;
    } else if (!strcmp(method, "PALY")) {
        mMethod = PLAY;
    } else if (!strcmp(method, "TEARDOWN")) {
        mMethod = TEARDOWN;
    } else {
        mMethod = NONE;
        return false;
    }

    if (strncmp(url, "rtsp://", 7) != 0) {
        return false;
    }

    uint16_t port = 0;
    char ip[64] = {0};
    char suffix[64] = {0};
    /*
    * hu: half unsigned, 即short int
    * %[^:]表示连续匹配不是':'的字符
    * 后面的:表示肯定有一个:
    * pattern: ip:port/path
    */
    if (sscanf(url + 7, "%[^:]:%hu/%s", ip, &port, suffix) == 3) {

    } else if (sscanf(url + 7, "%[^/]/%s", ip, suffix) == 2) {
        port = 554;  // 如果rtsp请求地址中无端口，默认获取的端口为554
    } else {
        return false;
    }
    mUrl = url;
    mSuffix = suffix;
    return true;  // 解析成功
}

bool RtspConnection::parseRequest() {
    // 解析第一行
    const char* crlf = mInputBuffer.findCRLF();
    if (crlf == nullptr) {
        mInputBuffer.retrieveAll();  // 该函数不是很理解？
        // 找不到换行，说明数据不完整/错误
        return false;
    }
    // 示例: OPTIONS rtsp://10.45.12.141:554/h264/ch1/main/av_stream RTSP/1.0
    bool ret = parseRequest1(mInputBuffer.peek(), crlf);
    if (ret == false) {
        mInputBuffer.retrieveAll();
        return false;
    } else {  // 解析成功，将这一行从mInputBuffer中移除（逻辑删除，只修改指针的位置）
        mInputBuffer.retrieveUntil(crlf + 2);
    }

    // 解析第一行之后的所有行
    crlf = mInputBuffer.findLastCrlf();
    if (crlf == nullptr) {
        mInputBuffer.retrieveAll();  // 报文非法，清空缓冲区
        return false;
    }
    ret = parseRequest2(mInputBuffer.peek(), crlf);
    if (ret == false) {
        mInputBuffer.retrieveAll();  // 解析失败，清空缓冲区
        return false;
    } else {
        mInputBuffer.retrieveUntil(crlf + 2);  // 解析成功，将解析过的数据清空
        return true;
    }
}

bool RtspConnection::parseRequest2(const char* begin, const char* end) {
    std::string message(begin, end);
    if (!parseCSeq(message)) {
        return false;
    }
    if (mMethod == OPTIONS) {
        return true;  // 没什么需要check的，直接返回true
    } else if (mMethod == DESCRIBE) {
        return parseDescribe(message);
    } else if (mMethod == SETUP) {
        return parseSetup(message);
    } else if (mMethod == PLAY) {
        return parsePlay(message);
    } else if (mMethod == TEARDOWN) {  // 表示终止会话，服务器端需要安全释放资源
        return true;
    }
    return false;
}

bool RtspConnection::parseCSeq(std::string& message) {
    std::size_t pos = message.find("CSeq");
    // LOG_DEBUG("pos: %d", pos);
    if (pos != std::string::npos) {  // npos是固定常量
        uint32_t cseq = 0;
        /**
         * %*[^:]: %u中*表示忽略匹配到的内容，不存到变量
         * 整体意思: 读取并忽略从CSeq开始到冒号前的所有内容
         */
        // LOG_DEBUG("message + pos: %s", message.c_str() + pos);
        sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);  // 注意这里有一个space
        mCSeq = cseq;
        return true;
    }
    return false;
}

bool RtspConnection::parseDescribe(std::string& message) {
    // TODO 不理解这里
    if (message.rfind("Accept") == std::string::npos
        || message.rfind("sdp") == std::string::npos) {
        return false;
    }
    return true;
}

bool RtspConnection::parseSetup(std::string& message) {
    mTrackId = MediaSession::TrackIdNone;
    std::size_t pos;
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        pos = mUrl.find(mStreamPrefix + std::to_string(i));
        if (pos != std::string::npos) {
            if (i == 0) {
                mTrackId = MediaSession::TrackId0;
            } else if (i == 1) {
                mTrackId = MediaSession::TrackId1;
            }
        }
    }
    if (mTrackId == MediaSession::TrackIdNone) {
        return false;
    }
    pos = message.find("Transport");
    if (pos != std::string::npos) {
        if ((pos = message.find("RTP/AVP/TCP")) != std::string::npos) {
            uint8_t rtpChannel, rtcpChannel;
            mIsRtpOverTcp = true;
            // 实例: Transport: RTP/AVP/TCP;unicast;interleaved=0-1
            // %hhu一个half是short int, 两个 half 就是8位int
            if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hhu-%hhu", &rtpChannel, &rtcpChannel) != 2) {
                return false;
            }
            mRtpChannel = rtpChannel;
        } else if ((pos = message.find("RTP/AVP")) != std::string::npos) {
            uint16_t rtpPort = 0, rtcpPort = 0;
            // 实例: Transport: RTP/AVP;unicast;client_port=63538-63539
            if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu", &rtpPort, &rtcpPort) != 2) {
                return false;
            } else if (message.find("multicast", pos) != std::string::npos) {
                return true;  // Q: 这啥意思?
                // A: 如果是组播，直接返回成功
            } else {
                return false;
            }
            mPeerRtpPort = rtpPort;
            mPeerRtcpPort = rtcpPort;
        } else {
            return false;
        }
        return true;
    }
    return false;
}

bool RtspConnection::parsePlay(std::string& message) {
    std::size_t pos = message.find("Session");
    if (pos != std::string::npos) {
        uint32_t sessionId = 0;
        if (sscanf(message.c_str() + pos, "%*[^:]: %u", &sessionId) != 1) {
            return false;  // 解析失败
        }
        return true;
    }
    return false;  // 没有Session字段
}

bool RtspConnection::handleCmdOption() {
    // 向缓冲区mBuffer最多写入sizeof(mBuffer)-1个字符
    snprintf(mBuffer, sizeof(mBuffer),
    "RTSP/1.0 200 OK\r\n"
    "CSeq: %u\r\n"
    "Public: DESCRIBE, ANNOUNCE, SETUP, PLAY, RECORD, PAUSE, GET_PARAMETER, TEARDOWN\r\n"
    "Server: %s\r\n"
    "\r\n", mCSeq, PROJECT_VERSION);

    if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
        return false;
    }
    return true;
}

bool RtspConnection::handleCmdDescribe() {
    MediaSession* session = mRtspServer->mSessMgr->getSession(mSuffix);
    if (!session) {
        LOG_ERROR("can't find session: %s", mSuffix.c_str());
        return false;
    }
    std::string sdp = session->generateSdpDescription();
    memset((void*)mBuffer, 0, sizeof(mBuffer));
    snprintf((char*)mBuffer, sizeof(mBuffer), 
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %u\r\n"
            "Content-Length: %u\r\n"
            "Content-Type: application/sdp\r\n"
            "\r\n"
            "%s",
            mCSeq, (unsigned int)sdp.size(), sdp.c_str());
    if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
        return false;
    }
    return true;
}

bool RtspConnection::handleCmdSetup() {
    char sessionName[100] = {0};
    // TODO 不懂这个mSuffix长什么样，sessionName长什么样
    LOG_DEBUG("mSuffix: %s", mSuffix.c_str());
    if (sscanf(mSuffix.c_str(), "%[^/]/", sessionName) != 1) {
        return false;
    }
    LOG_DEBUG("sessionName: %s", sessionName);
    MediaSession* session = mRtspServer->mSessMgr->getSession(sessionName);
    if (!session) {
        LOG_ERROR("can not find session: %s", sessionName);
        return false;
    }
    // mTrackId不合法 或者 已经有rtp或者rtcp的实例（表明已经setup过了）
    if (mTrackId >= MEDIA_MAX_TRACK_NUM || mRtpInstances[mTrackId] || mRtcpInstances[mTrackId]) {
        return false;
    }
    if (session->isStartMulticast()) {
        snprintf((char*)mBuffer, sizeof(mBuffer),
                "RTSP/1.0 200 OK\r\n"
                "CSeq: %d\r\n"
                "Transport: RTP/AVP;multicast;"
                "destination=%s;source=%s;port=%d-%d;ttl=255\r\n"
                "Session: %08x\r\n"
                "\r\n", 
                mCSeq, session->getMulticastDestAddr().c_str(),
                sockets::getLocalIp().c_str(), 
                session->getMulticastDestRtpPort(mTrackId),
                session->getMulticastDestRtpPort(mTrackId)+1,
                mSessionId);
    } else {
        if (mIsRtpOverTcp) {
            // 创建 rtp over tcp
            createRtpOverTcp(mTrackId, mClientFd, mRtpChannel);  // 在解析setup请求中完成了字段mTrackId、mRtpChannel、mIsRtpOverTcp的赋值
            mRtpInstances[mTrackId]->setSessionId(mSessionId);
            
            session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);
            snprintf((char*)mBuffer, sizeof(mBuffer),
                    "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Server: %s\r\n"
                    "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
                    "Session: %08x\r\n"
                    "\r\n", mCSeq, PROJECT_VERSION, mRtpChannel, mRtpChannel+1, mSessionId);
        } else {
            // 创建 rtp over udp
            if (createRtpRtcpOverUdp(mTrackId, mPeerIp, mPeerRtpPort, mPeerRtcpPort) != true) {
                LOG_ERROR("failed to createRtpRtcpOverUdp");
                return false;
            }
            mRtpInstances[mTrackId]->setSessionId(mSessionId);
            mRtcpInstances[mTrackId]->setSessionId(mSessionId);
            session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);

            snprintf((char*)mBuffer, sizeof(mBuffer),
                    "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Server: %s\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu\r\n"
                    "Session: %08x\r\n"
                    "\r\n", mCSeq, PROJECT_VERSION, mPeerRtpPort, mPeerRtcpPort, 
                    mRtpInstances[mTrackId]->getLocalPort(), mRtcpInstances[mTrackId]->getLocalPort(),
                    mSessionId);
        }
    }
    if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
        return false;
    }
    return true;
}

bool RtspConnection::handleCmdPlay() {
    snprintf((char*)mBuffer, sizeof(mBuffer),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "Server: %s\r\n"
            "Range: npt=0.000-\r\n"
            "Session: %08x; timeout=60\r\n"
            "\r\n", mCSeq, PROJECT_VERSION, mSessionId);
    if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
        return false;
    }
    for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
        if (mRtpInstances[i]) {
            mRtpInstances[i]->setAlive(true);
        }
        if (mRtcpInstances[i]) {
            mRtcpInstances[i]->setAlive(true);
        }
    }
    return true;
}

bool RtspConnection::handleCmdTeardown() {
    snprintf((char*)mBuffer, sizeof(mBuffer),
            "RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "Server: %s\r\n"
            "\r\n", mCSeq, PROJECT_VERSION);
    if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
        return false;
    }
    return true;
}

int RtspConnection::sendMessage(void* buf, int size) {
    LOG_INFO("%s", buf);
    int ret;
    mOutBuffer.append(buf, size);
    ret = mOutBuffer.write(mClientFd);
    mOutBuffer.retrieveAll();
    return ret;
}

int RtspConnection::sendMessage() {
    int ret = mOutBuffer.write(mClientFd);
    mOutBuffer.retrieveAll();
    return ret;
}
// TODO 不太懂这个handle
void RtspConnection::handleReadBytes() {
    if (mIsRtpOverTcp) {
        if (mInputBuffer.peek()[0] == '$') {
            handleRtpOverTcp();
            return;
        }
    }
    if (!parseRequest()) {
        LOG_ERROR("parseRequest error");
        goto disConnect;
    }
    switch(mMethod) {
        case OPTIONS:
            if (!handleCmdOption()) {
                goto disConnect;
            }
            break;
        case DESCRIBE:
            if (!handleCmdDescribe()) {
                goto disConnect;
            }
            break;
        case SETUP:
            if (!handleCmdSetup()) {
                goto disConnect;
            }
            break;
        case PLAY:
            if (!handleCmdPlay()) {
                goto disConnect;
            }
            break;
        case TEARDOWN:
            if (!handleCmdTeardown()) {
                goto disConnect;
            }
            break;
        default:
            goto disConnect;
    }
disConnect:
    handleDisConnect();
}

// 创建服务器上的Rtp实例(over tcp)
bool RtspConnection::createRtpOverTcp(MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel) {
    mRtpInstances[trackId] = RtpInstance::createNewOverTcp(sockfd, rtpChannel);
    return true;
}

void RtspConnection::handleRtpOverTcp() {
    int num = 0;
    while (true) {
        num += 1;
        uint8_t* buf = (uint8_t*)mInputBuffer.peek();
        // 前四个字节分别是
        // 1)魔方符号'$'; 2)channel; 3)rtp包大小的高8位; 4)rtp包大小的低8位
        uint8_t rtpChannel = buf[1];
        int16_t rtpSize = (buf[2] << 8) | buf[3];
        int16_t bufSize = 4 + rtpSize;
        if (mInputBuffer.readableBytes() < bufSize) {
            // 缓存数据小于一个RTP数据包的长度
            return;
        } else {
            if (0x00 == rtpChannel) {
                RtpHeader rtpHeader;
                parseRtpHeader(buf+4, &rtpHeader);
                LOG_INFO("num=%d, rtpSize=%d", num, rtpSize);
            } else if (0x01 == rtpChannel) {
                RtcpHeader rtcpHeader;
                parseRtcpHeader(buf+4, &rtcpHeader);
                LOG_INFO("num=%d, rtcpHeader.packetType=%d, rtpSize=%d", num, rtcpHeader.packetType, rtpSize);
            } else if (0x02 == rtpChannel) {
                RtpHeader rtpHeader;
                parseRtpHeader(buf+4, &rtpHeader);
                LOG_INFO("num=%d, rtpSize=%d", num, rtpSize);
            } else if (0x03 == rtpChannel) {
                RtcpHeader rtcpHeader;
                parseRtcpHeader(buf+4, &rtcpHeader);
                LOG_INFO("num=%d, rtcpHeader.packetType=%d, rtpSize=%d", num, rtcpHeader.packetType, rtpSize);
            }
            mInputBuffer.retrieve(bufSize);
        }
    }
}

// 创建服务器上的Rtp和Rtcp实例(over udp)
bool RtspConnection::createRtpRtcpOverUdp(MediaSession::TrackId trackId, std::string peerIp,
                                          uint16_t peerRtpPort, uint16_t peerRtcpPort) {
    int rtpSockfd, rtcpSockfd;
    uint16_t rtpPort, rtcpPort;
    bool ret;
    // 已经创建过了
    if (mRtpInstances[trackId] || mRtcpInstances[trackId]) {
        return false;
    }
    int i = 0;
    for (i = 0; i < 10; ++i) {
        rtpSockfd = sockets::createUdpSock();
        if (rtpSockfd < 0) {
            return false;
        }
        rtcpSockfd = sockets::createUdpSock();
        if (rtcpSockfd < 0) {
            sockets::close(rtpSockfd);
            return false;
        }

        uint16_t port = rand() & 0xfffe;
        if (port < 10000) {
            port += 10000;
        }
        rtpPort = port;
        rtcpPort = port + 1;

        ret = sockets::bind(rtpSockfd, "0.0.0.0", rtpPort);
        if (ret != true) {
            sockets::close(rtpSockfd);
            sockets::close(rtcpSockfd);
            continue;
        }
        ret = sockets::bind(rtcpSockfd, "0.0.0.0", rtcpPort);
        if (ret != true) {
            sockets::close(rtpSockfd);
            sockets::close(rtcpSockfd);
            continue;
        }
        break;
    }
    // 10次都失败了
    if (i == 10) {
        return false;
    }
    mRtpInstances[trackId] = RtpInstance::createNewOverUdp(rtpSockfd, rtpPort, peerIp, peerRtpPort);
    mRtcpInstances[trackId] = RtcpInstance::createNew(rtcpSockfd, rtcpPort, peerIp, peerRtcpPort);
    return true;
}