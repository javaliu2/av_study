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
#include <fcntl.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <string>
#include "logger.h"
#include "rtp.h"
#include "rtsp_base.h"

// 以下两行是MSVC(micro soft visual c++编译器)专属的预处理指令
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")  // 告诉编译器自动链接ws2_32.lib库
#pragma warning(disable : 4096)  // 关闭MSVC的4096号编译
#endif



// inline修饰的函数，建议编译器进行代码展开，而不是函数调用
static inline bool startCode001(char* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {  // 粗心, [2]==0, big bug
        return true;
    }
    return false;
}

static inline bool startCode0001(char* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {  // the same error
        return true;
    }
    return false;
}

static char* findNextStartCode(char* buf, int len) {
    if (len < 3) {
        return nullptr;
    }
    // 举例, len==6, 6-3==3, i<3的话, i最大取到2, 索引即2、3、4、5
    // 不能取到3，因为startCode0001函数中的判断会越界
    for (int i = 0; i < len - 3; ++i) {
        if (startCode001(buf) || startCode0001(buf)) {
            return buf;
        }
        ++buf;
    }
    // for结束的时候buf指向索引为3的字节
    // 判断最后三个字节
    if (startCode001(buf)) {
        return buf;
    }
    return nullptr;
}
static int getFrameFromH264File(FILE* fp, char* frame, int size) {
    int rSize, frameSize;
    char* nextStartCode;
    if (fp == nullptr)
        return -1;
    // function signature: 
    // size_t fread(void *DstBuf, size_t ElementSize, size_t Count, FILE* stream)
    // 从给定流stream读取数据(Count个元素，每个元素大小为ElementSize个字节)到DstBuf所指向的数组中
    // 返回实际读取到的元素个数
    rSize = fread(frame, 1, size, fp);
    if (!startCode001(frame) && !startCode0001(frame)) {
        return -1;
    }
    // 跳过第一个startCode，剩余的数据长度为rSize-3
    nextStartCode = findNextStartCode(frame + 3, rSize - 3);
    if (!nextStartCode) {
        return -1;
    } else {
        frameSize = nextStartCode - frame;
        /**
         * function signature: int fseek(FILE* stream, long Offset, int Origin)
         * 设置stream流中指针的指向，从Origin位置设置文件指针偏移Offset个字节，即进行指针的移动
         */
        fseek(fp, frameSize - rSize, SEEK_CUR);  // SEEK_CUR是当前指针的位置
    }
    return frameSize;
}
static int rtpSendH264Frame(int serverRtpSockfd, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize) {
    uint8_t naluFirstByte;
    int sendBytes = 0;
    int ret;
    // 这里传递进来的frame是不包括startCode的
    naluFirstByte = frame[0];
    #ifdef DEBUG
    LOG_INFO("frameSize=%d\n", frameSize);
    #endif
    // 这里的frameSize其实是一个NALU的大小
    if (frameSize <= RTP_MAX_PKT_SIZE) {
        // 一个NALU的大小小于RTP包所能支持的最大大小，所以将采用单一NALU模式，即将一个NALU放于一个RTP包中
        memcpy(rtpPacket->payload, frame, frameSize);  // 从frame拷贝frameSize字节的数据到rtpPacket->payload
        #ifdef DEBUG
        LOG_INFO("send frame:");
        for (int i = 0; i < frameSize; ++i) {
            printf("%02x ", frame[i] & 0xFF);
        }
        LOG_INFO("\nframe data display end");
        #endif
        ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, frameSize);
        if (ret < 0) {
            return -1;
        }
        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;
        // 后5位是NALU type
        if ((naluFirstByte & 0x1F == 7) || (naluFirstByte & 0x1F == 8)) {  // 如果是SPS、PPS就不需要加时间戳
            goto out;
        }
    } else {  // NALU大小大于RTP所能支持的最大大小，故采取分片模式
        int pktNum = frameSize / RTP_MAX_PKT_SIZE;
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE;
        int pos = 1;
        for (int i = 0; i < pktNum; ++i) {
            // 0x60对应的二进制0110 0000，读取NRI值
            rtpPacket->payload[0] = (naluFirstByte & 0x60) | 28;  // FU indicator, 读取原始的NRI值再置indicator的type为28
            rtpPacket->payload[1] = naluFirstByte & 0x1F;  // FU header, 读取原始的NALU type

            if (i == 0) {
                // 第一个包的第二个字节FU header要置S位，0x80=1000 0000(b)
                rtpPacket->payload[1] |= 0x80;
            } else if (remainPktSize == 0 && i == pktNum - 1) {
                // 只有pktNum个包，没有不足RTP_MAX_PKT_SIZE个字节的剩余的数据
                rtpPacket->payload[1] |= 40;  // 置E位，0x40=0100 0000(b)
            }
            // h264码流数据大小为RTP_MAX_PKT_SIZE，在RTP payload中还有两字节的FU数据，故RTP payload数据大小为RTP_MAX_PKT_SIZE + 2
            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);  // pos==1，跳过原始码流的第一个字节，即NALU header，因为该header的数据被包括在FU indicator和FU header中
            ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, RTP_MAX_PKT_SIZE + 2);  // +2表示FU indicator和header的两字节
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
            ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, remainPktSize+2);
            if (ret < 0) {
                return -1;
            }
            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }
    // rtpPacket->rtpHeader.timestamp += 90000 / 25;
    rtpPacket->rtpHeader.timestamp += 1200000 / 25;
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