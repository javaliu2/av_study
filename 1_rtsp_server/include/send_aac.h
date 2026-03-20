/**
 * 一共56bit, 7byte
 */
#include <cstdint>
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

int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res);

int rtpSendAACFrame(int socket, const char* ip, uint16_t port, struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize);

