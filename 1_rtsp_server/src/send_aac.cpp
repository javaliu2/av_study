#include "send_aac.h"
#include "rtp.h"
#include <cstring>
#include <logger.h>

int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
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

int rtpSendAACFrame(int socket, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize) {
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
    rtpPacket->rtpHeader.timestamp += 1024;

    return 0;
}