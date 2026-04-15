#pragma once
#include <stdint.h>
#include <stdlib.h>

#define RTP_VERSION 2

#define RTP_PAYLOAD_TYPE_H264 96
#define RTP_PAYLOAD_TYPE_AAC 97

#define RTP_HEADER_SIZE 12
#define RTP_MAX_PKT_SIZE 1400

struct RtpHeader {
    // byte 0
    uint8_t csrcLen:4;// contributing source identifiers count
    uint8_t extension:1;
    uint8_t padding:1;
    uint8_t version:2;

    // byte 1
    uint8_t payloadType:7;
    uint8_t marker:1;
    
    // bytes 2,3
    uint16_t seq;
    
    // bytes 4-7
    uint32_t timestamp;
    
    // bytes 8-11
    uint32_t ssrc;

    // data
    uint8_t payload[0];
};
/**
 * +-+-+-+-+-+-+-+-+-+-+-+-
 * 
 * +-+-+-+-+-+-+-+-+-+-+-+-
 */
struct RtcpHeader {
    // byte 0
    uint8_t rc : 5;  // reception report count
    uint8_t padding : 1;
    uint8_t version : 2;
    // byte 1
    uint8_t packetType;
    // byte 2, 3
    uint16_t length;
};

class RtpPacket {
public:
    RtpPacket();
    ~RtpPacket();
public:
    uint8_t* mBuf;  // 4+rtpHeader+rtpBody, 供使用TCP发送数据包使用
    uint8_t* mBuf4;  // rtpHeader+rtpBody，供使用UDP发送数据包使用
    RtpHeader* const mRtpHeader;  // Q: const为什么要这样设置，是不是写错位置了？
    // A: 没有写错，这样设置，表示mRtpHeader一经赋值就不可修改（指向其他内存地址）
    int mSize;  // rtpHeader + rtpBody
};

void parseRtpHeader(uint8_t* buf, struct RtpHeader* rtpHeader);
void parseRtcpHeader(uint8_t* buf, struct RtcpHeader* rtcpHeader);


