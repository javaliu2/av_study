// code from https://gitee.com/Vanishi/BXC_RtspServer_study/blob/master/study3/main.cpp
// used just for own study.

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
#include "rtp.h"
#include "logger.h"
#include "rtsp_base.h"

/**
 * 一共56bit, 7byte
 */
struct AdtsHeader {
    unsigned int syncword;  // 12 bit同步字，0xFFF，标志ADTS帧的开始
    uint8_t id;  // 1 bit，0代表MPEG-4，1代表MPEG-2
    uint8_t layer;  // 2 bit, 必须为0
    uint8_t protectionAbsent;  // 1 bit, 1代表没有CRC，0代表有CRC
    
    uint8_t profile;  // 2 bit, AAC级别（MPEG-2 AAC中定义了3种profile，MPEG-4中定义了6种profile）
    uint8_t samplingFreqIndex;  // 4 bit, 采样率表索引
    uint8_t privateBit;  // 1 bit, 编码时设置为0，解码时忽略
    uint8_t channelCfg;  // 3 bit, 声道数量
    uint8_t originalCopy;  // 1 bit, 编码时设置为0，解码时忽略
    uint8_t home;  // 1 bit, 编码时设置为0，解码时忽略
    uint8_t copyrightIdentificationBit;  // 1 bit, 编码时设置为0，解码时忽略
    uint8_t copyrightIdentificationStart;  // 1 bit, 编码时设置为0，解码时忽略
    unsigned int aacFrameLength;  // 13 bit, 一个ADTS帧的长度，包括ADTS头和AAC原始流的大小
    unsigned int adtsBufferFullness;  // 11 bit, 缓冲区充满度，0x7FF说明是码流可变的码流，不需要此字段。CBR可能需要此字段，不同编码器使用情况不同。这个在使用音频编码的时候需要注意

    /**
     * 表示ADTS帧中有多少个AAC帧，AAC帧个数等于numberOfRawDataBlockInFrame + 1
     * 一般，一个AAC帧包括1024个采样以及相关数据（注意区分AAC帧和采样的概念）
     */
    uint8_t numberOfRawDataBlockInFrame;  // 2 bit
};
/**
 * |+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
 * |
 * 
 */
static int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
    static int frame_number = 0;
    memset(res, 0, sizeof(*res));  // equals to sizeof(AdtsHeader);
    if (in[0] == 0xFF && ((in[1] & 0xF0) == 0xF0)) {  // 前12 bit是0xFFF
        res->id = ((uint8_t)in[1] & 0x08) >> 3;  // 第13bit是id，在第二个字节的第5位上，0000 1000十进制值为8，通过与运算获取到该位为1时对应的二进制数值，通过移位运算获取该位的值
        res->layer = ((uint8_t)in[1] & 0x06) >> 1;  // 第14、15bit是layer，在第二个字节的6、7位上，0000 0110(b)=6(decimal)
        res->protectionAbsent = (uint8_t)in[1] & 0x01;

        res->profile = ((uint8_t)in[2] & 0xC0) >> 6;  // 第三个字节前2位, 1100 0000(b) = 0xC0
        res->samplingFreqIndex = ((uint8_t)in[2] & 0x3C) >> 2;  // 第三个字节第3、4、5、6位，0011 1100(b) = 0x3C
        res->privateBit = ((uint8_t)in[2] & 0x02) >> 1;  // 第三个字节第7位, 0000 0010(b) = 0x02

        res->channelCfg = (((uint8_t)in[2] & 0x01) << 2) | (((unsigned int)in[3] & 0xC0) >> 6);  // 3 bit, 第三个字节第8位 + 第四个字节前2位
        res->originalCopy = ((uint8_t)in[3] & 0x20) >> 5;  // 第四个字节第3位
        res->home = ((uint8_t)in[3] & 0x10) >> 4;  // 第四个字节第4位
        res->copyrightIdentificationBit = ((uint8_t)in[3] & 0x08) >> 3;  // 第四个字节第5位
        res->copyrightIdentificationStart = ((uint8_t)in[3] & 0x04) >> 2;  // 第四个字节第6位
        
        res->aacFrameLength = ((unsigned int)in[3] & 0x03) << 11 | (((unsigned int)in[4] & 0xFF) << 3) | (((unsigned int)in[5] & 0xE0) >> 5);  // 13 bit，第四个字节第7、8位 + 第五个字节全部8位 + 第六个字节前3位
        res->adtsBufferFullness = (((unsigned int)in[5] & 0x1F) << 6) | (((unsigned int)in[6] & 0xFC) >> 2);  // 11 bit，第六个字节后5位 + 第七个字节前6位
        res->numberOfRawDataBlockInFrame = (uint8_t)in[6] & 0x03;  // 第七个字节第7、8位
        return 0;
    } else {
        LOG_ERROR("failed to parse adts header.\n");
        return -1;
    }
}

static int rtpSendAACFrame(int socket, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize) {
    int ret;
    // RTP负载，前四个字节是固定的，后面追加AAC数据
    rtpPacket->payload[0] = 0x00;
    rtpPacket->payload[1] = 0x10;
    rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5;  // 第三个字节保存AAC数据大小的高8位
    rtpPacket->payload[3] = (frameSize & 0x1F) << 3;  // 第四个字节的高5位保存数据大小的低5位

    memcpy(rtpPacket->payload + 4, frame, frameSize);
    ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, frameSize + 4);
    if (ret < 0) {
        LOG_ERROR("failed to send rtp packet");
        return -1;
    }
    rtpPacket->rtpHeader.seq++;
    /**
     * 如果采样频率为44100
     * 一般AAC每1024个采样为一帧
     * 所以1秒就有 44100 / 1024 ~= 43帧
     * 单位时间增量 44100 / 43 ~= 1025
     * 一帧的时长 1000 / 43 ~= 23ms
     */
    rtpPacket->rtpHeader.timestamp += 1025;

    return 0;
}

int handleCmd_DESCRIBE_aac(char* result, int cseq, const char* url) {
    char sdp[500];
    char localIp[64] = {0};
    
    sscanf(url, "rtsp://%63[^:/]", localIp);
    printf("%s\n", localIp);
    sprintf(sdp, "v=0\r\no=- 9%ld 1 IN IP4 %s\r\nt=0 0\r\na=control:*\r\nm=audio 0 RTP/AVP 97\r\na=rtpmap:97 mpeg4-generic/44100/2\r\na=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
        "a=control:track0\r\n", 
        time(NULL), localIp);
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
    "Content-Base: %s\r\n"
    "Content-type: application/sdp\r\n"
    "Content-length: %zu\r\n\r\n"
    "%s",
    cseq, url, strlen(sdp), sdp);
    return 0;
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
            if (handleCmd_DESCRIBE_aac(sBuf, CSeq, url)) {
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
                LOG_INFO("fread ret=%d", ret);
                if (parseAdtsHeader(frame, &adtsHeader) < 0) {
                    LOG_ERROR("parseAdtsHeader error");
                    break;
                }
                ret = fread(frame, 1, adtsHeader.aacFrameLength-7, fp);  // aacFrameLength包括头部和aac原始数据，因此这里-7，只读取aac原始数据
                if (ret <= 0) {
                    LOG_ERROR("fread aac raw data error");
                    break;
                }
                // 这里写错了，应该是clientRtpPort，而不是clientPort。端口错了，传输错地方了，肯定就不对了
                rtpSendAACFrame(serverRtpSockfd, clientIP, clientRtpPort, rtpPacket, frame, adtsHeader.aacFrameLength-7);
                Sleep(23);
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