#pragma once
#include <stdint.h>

#define RTP_VERSION 2

#define RTP_PAYLOAD_TYPE_H264 96
#define RTP_PAYLOAD_TYPE_AAC 97

#define RTP_HEADER_SIZE 12
#define RTP_MAX_PKT_SIZE 1400

/**
 *  0                 1                 2                 3
 *  7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0 | 7 6 5 4 3 2 1 0
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|   CC   | M|      PT      |          sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       timestamp                                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               synchronization source(SSRC) identifier               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               contributing source(CSRC) identifier                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  */
struct RtpHeader {
    /* byte 0，从低位到高位 */
    uint8_t csrcLen : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;

    /* byte 1 */
    uint8_t payloadType : 7;
    uint8_t marker : 1;

    /* byte 2,3 */
    uint16_t seq;

    /* byte 4-7 */
    uint32_t timestamp;

    /* byte 8-11 */
    uint32_t ssrc;
};
static void func() {
    sizeof(RtpPacket);  // 12
    sizeof(RtpHeader);  // 12
}
struct RtpPacket {
    struct RtpHeader rtpHeader;
    uint8_t payload[0];  // 不占用字节
};

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker, uint16_t seq,
uint32_t timestamp, uint32_t ssrc);

int rtpSendPacketOverTcp(int clientSockfd, struct RtpPacket* rtpPacket, uint32_t dataSize);
int rtpSendPaacketOverUdp(int serverRtpSockfd, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize);


