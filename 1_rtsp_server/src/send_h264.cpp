#include <cstdio>
#include <cstdint>
#include "rtp.h"
#include "send_h264.h"
#include <cstring>

// inline修饰的函数，建议编译器进行代码展开，而不是函数调用
bool startCode001(char* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {  // 粗心, [2]==0, big bug
        return true;
    }
    return false;
}

bool startCode0001(char* buf) {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {  // the same error
        return true;
    }
    return false;
}

char* findNextStartCode(char* buf, int len) {
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

int getFrameFromH264File(FILE* fp, char* frame, int size) {
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

int rtpSendH264Frame(int serverRtpSockfd, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize) {
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
    rtpPacket->rtpHeader.timestamp += 90000 / 25;
    // rtpPacket->rtpHeader.timestamp += 1200000 / 25;
    out:
    return sendBytes;
}
