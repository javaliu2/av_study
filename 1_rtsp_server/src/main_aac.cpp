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

#define SERVER_PORT     8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533
#define BUF_MAX_SIZE    (1024*1024)
#define AAC_FILE_NAME   "../data/test-long.aac"

static int createTcpSocket() {
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int createUdpSocket() {
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int bindSocketAddr(int sockfd, const char* ip, int port) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
        return -1;

    return 0;
}
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

        res->channelCfg = ((uint8_t)in[2] & 0x01) << 2 | ((uint8_t)in[3] & 0xC0) >> 6;  // 3 bit, 第三个字节第8位 + 第四个字节前2位
        res->originalCopy = ((uint8_t)in[3] & 0x20) >> 5;  // 第四个字节第3位
        res->home = ((uint8_t)in[3] & 0x10) >> 4;  // 第四个字节第4位
        res->copyrightIdentificationBit = ((uint8_t)in[3] & 0x08) >> 3;  // 第四个字节第5位
        res->copyrightIdentificationStart = ((uint8_t)in[3] & 0x04) >> 2;  // 第四个字节第6位
        
        res->aacFrameLength = ((uint8_t)in[3] & 0x03) << 11 | ((uint8_t)in[4] << 3) | (((uint8_t)in[5] & 0xE0) >> 5);  // 13 bit，第四个字节第7、8位 + 第五个字节全部8位 + 第六个字节前3位
        res->adtsBufferFullness = (((uint8_t)in[5] & 0x1F) << 6) | (((uint8_t)in[6] & 0xFC) >> 2);  // 11 bit，第六个字节后5位 + 第七个字节前6位
        res->numberOfRawDataBlockInFrame = (uint8_t)in[6] & 0x03;  // 第七个字节第7、8位
        return 0;
    } else {
        LOG_INFO("failed to parse adts header.\n");
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

int main() {
    return 0;
}