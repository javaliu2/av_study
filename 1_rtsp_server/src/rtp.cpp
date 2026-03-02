#include "rtp.h"
#include "logger.h"
#include <sys/types.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker, uint16_t seq,
uint32_t timestamp, uint32_t ssrc) {
    rtpPacket->rtpHeader.csrcLen = csrcLen;
    rtpPacket->rtpHeader.extension = extension;
    rtpPacket->rtpHeader.padding = padding;
    rtpPacket->rtpHeader.version = version;
    rtpPacket->rtpHeader.payloadType = payloadType;
    rtpPacket->rtpHeader.marker = marker;
    rtpPacket->rtpHeader.seq = seq;
    rtpPacket->rtpHeader.timestamp = timestamp;
    rtpPacket->rtpHeader.ssrc = ssrc;
}
/**
 * @param dataSize rtp包负载数据的大小
 */
int rtpSendPacketOverTcp(int clientSockfd, struct RtpPacket* rtpPacket, uint32_t dataSize) {
    // rtpPacket->rtpHeader.seq = htons(rtpPacket->rtpHeader.seq);
    // rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    // rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    uint32_t rtpSize32 = RTP_HEADER_SIZE + dataSize;  // rtp包的大小
    if (rtpSize32 > 65535) {
        LOG_ERROR("RTP packet too large: %u\n", rtpSize32);
        return -1;
    }
    uint16_t rtpSize = (uint16_t)rtpSize32;
    char* tempBuf = (char*)malloc(4 + rtpSize);
    if (!tempBuf) {
        LOG_ERROR("malloc failed\n");
        return -1;
    }
    // 使用TCP发送RTP包时必须加4字节前缀
    tempBuf[0] = 0x24;  // '$', RTSP over TCP的标志
    tempBuf[1] = 0x00;  // RTP通道，一般0是RTP，1是RTCP
    tempBuf[2] = (rtpSize >> 8) & 0xFF;  // RTP包长度 高8位
    tempBuf[3] = rtpSize & 0xFF;  // RTP包长度 低8位
    
    struct RtpPacket netPkt = *rtpPacket;  // copy packet
    netPkt.rtpHeader.seq = htons(netPkt.rtpHeader.seq);
    netPkt.rtpHeader.timestamp = htonl(netPkt.rtpHeader.timestamp);
    netPkt.rtpHeader.ssrc = htonl(netPkt.rtpHeader.ssrc);
    memcpy(tempBuf + 4, &netPkt, rtpSize);  // 将&netPkt内存处的rtpSize个字节拷贝到tempBuf[4]处

    int ret = send(clientSockfd, tempBuf, 4 + rtpSize, 0);
    // 后续逻辑涉及到对这些字段的修改，所以需要改为主机序
    // rtpPacket->rtpHeader.seq = ntohs(rtpPacket->rtpHeader.seq);
    // rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    // rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);
    // G-bro建议拷贝packet，然后直接发送，也不需要‘回溯’了

    free(tempBuf);
    tempBuf = NULL;

    return ret;
}
int rtpSendPaacketOverUdp(int serverRtpSockfd, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize)  {
    struct sockaddr_in addr;
    int ret;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    struct RtpPacket netPkt = *rtpPacket;
    netPkt.rtpHeader.seq = htons(netPkt.rtpHeader.seq);
    netPkt.rtpHeader.timestamp = htonl(netPkt.rtpHeader.timestamp);
    netPkt.rtpHeader.ssrc = htonl(netPkt.rtpHeader.ssrc);

    ret = sendto(serverRtpSockfd, (char*)&netPkt, dataSize + RTP_HEADER_SIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
    return ret;
}