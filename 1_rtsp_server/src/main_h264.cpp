/**
 * code refenence from https://gitee.com/Vanishi/BXC_RtspServer_study/blob/master/study1/main.cpp
 * 仅用于学习
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <string>
#include "logger.h"
#include "rtp.h"
#include "rtsp_base.h"
#include "send_h264.h"
#include "connection.h"
// 以下两行是MSVC(micro soft visual c++编译器)专属的预处理指令
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")  // 告诉编译器自动链接ws2_32.lib库
#pragma warning(disable : 4096)  // 关闭MSVC的4096号编译
#endif

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
        std::string recvStr = rBuf;
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
            // 服务器通过RTP包推送h264码流数据
            int frameSize, startCodeBit;
            char* frame = (char*)malloc(500000);
            struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(500000);
            const char* file_path = "../resource/test.h264";
            FILE* fp = fopen(file_path, "rb");
            if (!fp) {
                LOG_ERROR("读取文件%s失败\n", file_path);
                break;
            }
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_H264, 0, 0, 0, 0X88923423);
            printf("start play.\n");
            printf("client ip: %s\n", clientIP);
            printf("client port:%d\n", clientPort);
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
                rtpSendH264Frame(serverRtpSockfd, clientIP, clientRtpPort, rtpPacket, frame + startCodeBit, frameSize);
                Sleep(1);
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
    // const char* url = "rtsp://192.168.1.10:8554/live";
    // handleCmd_DESCRIBE(0, 0, url);
    // const char* url2 = "rtsp://example.com/live";
    // handleCmd_DESCRIBE(0, 0, url2);

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