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
        printf("calling function: %s; rBuf = '%s' \n", __FUNCTION__, rBuf);  // 系统宏__FUNCTION__，用于输出当前调用的函数名

        const char* sep = "\r\n";
        char* line = strtok(rBuf, sep);  // 对字符串rBuf按照分隔符sep进行分割
        while (line) {
            // strstr: 在line中查找子字符串substr第一次出现的位置，存在返回char*；如果不存在这样的substr，返回NULL
            if (strstr(line, "OPTIONS") || strstr(line, "DESCRIBE") || strstr(line, "SETUP") || strstr(line, "PLAY")) {
                if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
                    LOG_ERROR("parse method,url,version failed\n");
                }
            } else if (strstr(line, "CSeq") ) {
                // sscanf返回成功匹配和赋值的个数
                if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
                    LOG_ERROR("parse CSeq failed\n");
                }
            } else if (!strncmp(line, "Transport:", strlen("Transport:"))) {  // 比较str和str2至多前n个字符大小
                if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n", &clientRtpPort, &clientRtcpPort) != 2) {
                    LOG_ERROR("parse Transport failed\n");
                }
            }
            line = strtok(NULL, sep);  // 继续分割之前没有处理完的字符串
        }

        if (!strcmp(method, "OPTIONS")) {  // 相等返回0
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
            // 建立两个协议对应的传输通道
            serverRtpSockfd = createUdpSocket();
            serverRtcpSockfd = createUdpSocket();
            if (serverRtpSockfd < 0 || serverRtcpSockfd < 0) {
                LOG_ERROR("filed to setup rtp or rtcp socket\n");
                break;
            }
            if (bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT) < 0 ||
                bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT) < 0) {
                LOG_ERROR("failed to bind rtp or rtcp socket with according addr\n");
                break;
            }
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
            // 服务器通过RTP包推送AAC码流数据
            struct AdtsHeader adtsHeader;
            struct RtpPacket* rtpPacket;
            uint8_t* frame;
            int ret;
            const char* file_name = "../resource/test.aac";
            FILE* fp = fopen(file_name, "rb");
            if (!fp) {
                LOG_ERROR("读取 %s 失败", file_name);
                break;
            }
            frame = (uint8_t*)malloc(5000);
            rtpPacket = (struct RtpPacket*)malloc(5000);
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_AAC, 1, 0, 0, 0x32411);  // 负载的数据类型是AAC
            while (true) {
                ret = fread(frame, 1, 7, fp);  // 从aac文件中读取7个字节，即adts头部数据
                if (ret <= 0) {
                    LOG_ERROR("fread adts header error");
                    break;
                }
                #ifdef DEBUG
                LOG_INFO("fread ret=%d", ret);
                #endif
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
                ret = fread(frame, 1, adtsHeader.aacFrameLength-headerSize, fp);  // aacFrameLength包括头部和aac原始数据，因此这里-7，只读取aac原始数据
                if (ret <= 0) {
                    LOG_ERROR("fread aac raw data error");
                    break;
                }
                // 这里写错了，应该是clientRtpPort，而不是clientPort。端口错了，传输错地方了，肯定就不对了
                rtpSendAACFrame(serverRtpSockfd, clientIP, clientRtpPort, rtpPacket, frame, adtsHeader.aacFrameLength-headerSize);
                // Sleep(1024 * 1000 / 44100);  // 卡顿，可能就是由于发送时间太慢
                Sleep(1);  // 正常播放，无停顿、噪声
            }
            free(frame);
            free(rtpPacket);
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