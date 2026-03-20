// code from https://gitee.com/Vanishi/BXC_RtspServer_study/blob/master/study3/main.cpp
// used just for own study.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <windows.h>
#include "rtp.h"
#include "logger.h"
#include "rtsp_base.h"
#include "connection.h"
#include "send_aac.h"
#include "send_h264.h"
#include <thread>


static int rtpSendAACFrame(int clientSocketfd, RtpPacket *rtpPacket, uint8_t *frame, uint32_t frameSize) {
    int ret;

    rtpPacket->payload[0] = 0x00;
    rtpPacket->payload[1] = 0x10;
    rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5; //高8位
    rtpPacket->payload[3] = (frameSize & 0x1F) << 3; //低5位

    memcpy(rtpPacket->payload + 4, frame, frameSize);

    ret = rtpSendPacketOverTcp(clientSocketfd, rtpPacket, frameSize + 4, 0x02);  // 0-1分别是视频的rtp和rtcp channel，2-3分别是音频的rtp和rtcp channel
    if (ret < 0) {
        printf("failed to send rtp packet\n");
        return -1;
    }

    rtpPacket->rtpHeader.seq++;
    rtpPacket->rtpHeader.timestamp += 1024;
    return 0;
}

static int rtpSendH264Frame(int clientSocketfd, RtpPacket *rtpPacket, char *frame, uint32_t frameSize) {
    uint8_t naluFirstByte;
    int sendBytes = 0;
    int ret;
    naluFirstByte = frame[0];
    if (frameSize <= RTP_MAX_PKT_SIZE) {
        memcpy(rtpPacket->payload, frame, frameSize);
        ret = rtpSendPacketOverTcp(clientSocketfd, rtpPacket, frameSize, 0x00);
        if (ret < 0) {
            return -1;
        }
        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;
        if ((naluFirstByte & 0x1F == 7) || (naluFirstByte & 0x1F == 8)) {  // 如果是SPS、PPS就不需要加时间戳
            goto out;
        }
    } else {  // NALU大小大于RTP所能支持的最大大小，故采取分片模式
        int pktNum = frameSize / RTP_MAX_PKT_SIZE;
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE;
        int pos = 1;
        for (int i = 0; i < pktNum; ++i) {
            rtpPacket->payload[0] = (naluFirstByte & 0x60) | 28;
            rtpPacket->payload[1] = naluFirstByte & 0x1F;

            if (i == 0) {
                rtpPacket->payload[1] |= 0x80;
            } else if (remainPktSize == 0 && i == pktNum - 1) {
                rtpPacket->payload[1] |= 40;
            }
            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);
            ret = rtpSendPacketOverTcp(clientSocketfd, rtpPacket, RTP_MAX_PKT_SIZE + 2, 0x00);
            if (ret < 0) {
                return -1;
            }
            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
            pos += RTP_MAX_PKT_SIZE;
        }
        // 发送剩余的数据，至多一个包
        if (remainPktSize > 0) {
            rtpPacket->payload[0] = (naluFirstByte & 0x60) | 28;
            rtpPacket->payload[1] = naluFirstByte & 0x1F;
            rtpPacket->payload[1] |= 0x40;

            memcpy(rtpPacket->payload+2, frame+pos, remainPktSize);
            ret = rtpSendPacketOverTcp(clientSocketfd, rtpPacket, remainPktSize+2, 0x00);  // 0是视频的rtp channel
            if (ret < 0) {
                return -1;
            }
            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }
    rtpPacket->rtpHeader.timestamp += 90000 / 25;
    out:
    return sendBytes;
}

static void doClient(int clientSockfd, const char* clientIP, int clientPort) {
    char method[40];
    char url[100];
    char version[40];
    int CSeq;

    int serverRtpSockfd = -1, serverRtcpSockfd = -1;
    int clientRtpPort, clientRtcpPort;
    char* rBuf = (char*)malloc(BUF_MAX_SIZE);
    char* sBuf = (char*)malloc(BUF_MAX_SIZE);

    while (true) {
        int recvLen;

        recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
        if (recvLen <= 0)
            break;
        rBuf[recvLen] = '\0';
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        printf("calling function: %s; rBuf = '%s' \n", __FUNCTION__, rBuf);
        const char* sep = "\r\n";
        char* line = strtok(rBuf, sep);
        while (line) {
            if (strstr(line, "OPTIONS") || strstr(line, "DESCRIBE") || strstr(line, "SETUP") || strstr(line, "PLAY")) {
                if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
                    LOG_ERROR("parse method,url,version failed\n");
                }
            } else if (strstr(line, "CSeq") ) {
                if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
                    LOG_ERROR("parse CSeq failed\n");
                }
            } else if (!strncmp(line, "Transport:", strlen("Transport:"))) {
                if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n", &clientRtpPort, &clientRtcpPort) != 2) {
                    LOG_ERROR("parse Transport failed\n");
                }
            }
            line = strtok(NULL, sep);
        }

        if (!strcmp(method, "OPTIONS")) {
            if (handleCmd_OPTIONS(sBuf, CSeq)) {
                LOG_ERROR("failed to handle options\n");
                break;
            }
        } else if (!strcmp(method, "DESCRIBE")) {
            if (handleCmd_DESCRIBE(sBuf, CSeq, url)) {
                LOG_ERROR("failed to handle describe\n");
                break;
            }
        } else if (!strcmp(method, "SETUP")) {
            if (handleCmd_SETUP(sBuf, CSeq, clientRtpPort)) {
                LOG_ERROR("failed to handle setup\n");
                break;
            }
            // 不需要建立通道，因为rtp和rtcp均是通过tcp进行数据传输
        } else if (!strcmp(method, "PLAY")) {
            if (handleCmd_PLAY(sBuf, CSeq)) {
                LOG_ERROR("failed to handle play\n");
                break;
            }
        } else {
            LOG_INFO("未定义的method = %s\n", method);
            break;
        }
        printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
        printf("%s sBuf = %s\n", __FUNCTION__, sBuf);
        
        send(clientSockfd, sBuf, strlen(sBuf), 0);

        if (!strcmp(method, "PLAY")) {
            // 服务器通过RTP包推送音视频数据
            std::thread t1([&]() {
                int frameSize, startCodeBit;
                char* frame = (char*)malloc(500000);
                struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(500000);
                const char* file_path = "../resource/test2.h264";
                FILE* fp = fopen(file_path, "rb");
                if (!fp) {
                    LOG_ERROR("读取文件%s失败\n", file_path);
                    return;
                }
                rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0X88923423);
                while (true) {
                    frameSize = getFrameFromH264File(fp, frame, 500000);
                    if (frameSize < 0) {
                        LOG_INFO("读取%s结束, frameSize=%d\n", file_path, frameSize);
                        break;
                    }
                    if (startCode001(frame)) {
                        startCodeBit = 3;
                    } else {
                        startCodeBit = 4;
                    }
                    frameSize -= startCodeBit;
                    rtpSendH264Frame(clientSockfd, rtpPacket, frame + startCodeBit, frameSize);
                    Sleep(1);
                }
                free(frame);
                free(rtpPacket);
            });
            std::thread t2([&](){
                struct AdtsHeader adtsHeader;
                struct RtpPacket* rtpPacket;
                uint8_t* frame;
                int ret;
                const char* file_name = "../resource/test2.aac";
                FILE* fp = fopen(file_name, "rb");
                if (!fp) {
                    LOG_ERROR("读取 %s 失败", file_name);
                    return;
                }
                frame = (uint8_t*)malloc(5000);
                rtpPacket = (struct RtpPacket*)malloc(5000);
                rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_AAC, 1, 0, 0, 0x32411);
                while (true) {
                    ret = fread(frame, 1, 7, fp);
                    if (ret <= 0) {
                        LOG_ERROR("fread adts header error");
                        break;
                    }
                    if (parseAdtsHeader(frame, &adtsHeader) < 0) {
                        LOG_ERROR("parseAdtsHeader error");
                        break;
                    }
                    // 判断是否有CRC
                    int headerSize = 7;
                    if (!adtsHeader.protectionAbsent) {
                        ret = fread(frame + 7, 1, 2, fp);
                        if (ret != 2) {
                            break;
                        }
                        headerSize = 9;
                        LOG_INFO("header size is 9");
                    }
                    ret = fread(frame, 1, adtsHeader.aacFrameLength-headerSize, fp);
                    if (ret <= 0) {
                        LOG_ERROR("fread aac raw data error");
                        break;
                    }
                    rtpSendAACFrame(clientSockfd, rtpPacket, frame, adtsHeader.aacFrameLength-headerSize);
                    Sleep(1);
                }
                free(frame);
                free(rtpPacket);
            });
            t1.join();
            t2.join();
            break;
        }

        memset(method, 0, sizeof(method) / sizeof(char));
        memset(url, 0, sizeof(url) / sizeof(char));
        CSeq = 0;
    }

    closesocket(clientSockfd);
    if (serverRtpSockfd) {
        closesocket(serverRtpSockfd);
    }
    if (serverRtcpSockfd) {
        closesocket(serverRtcpSockfd);
    }
    free(rBuf);
    free(sBuf);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("PC Server Socket Start Up Error\n");
        return -1;
    }
    int serverSockfd;
    serverSockfd = createTcpSocket();
    if (serverSockfd < 0) {
        WSACleanup();
        printf("failed to create tcp socket\n");
        return -1;
    }
    if (bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT) < 0) {
        printf("failed to bind addr\n");
        return -1;
    }
    if (listen(serverSockfd, 10) < 0) {
        printf("failed to listen\n");
        return -1;
    }
    printf("%s rtsp:127.0.0.1:%d\n", __FILE__, SERVER_PORT);

    while (true) {
        int clientSockfd;
        char clientIp[40];
        int clientPort;

        clientSockfd = acceptClient(serverSockfd, clientIp, &clientPort);
        if (clientSockfd < 0) {
            printf("failed to accept client connect\n");
            return -1;
        }
        printf("accept client; client ip: %s, port: %d\n", clientIp, clientPort);
        doClient(clientSockfd, clientIp, clientPort);
    }
    closesocket(serverSockfd);
    return 0;
}