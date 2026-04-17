#pragma once
#include <string>
#include "MediaSource.h"

class AACFileMediaSource : public MediaSource {
public:
    static AACFileMediaSource* createNew(UsageEnvironment* env, const std::string& file);

    AACFileMediaSource(UsageEnvironment* env, const std::string& file);
    ~AACFileMediaSource() override;

protected:
    void handleTask() override;

private:
    struct AdtsHeader {
        unsigned int syncword;  // 12 bit同步字，说明一个adts帧的开始
        unsigned int id;  // 1 bit MPEG标识符，0 for MPEG-4, 1 for MPEG-2
        unsigned int layer;  // 2 bit 总是'00'
        unsigned int protectionAbsent;  // 1 bit，1表示没有crc，0表示有crc
        unsigned int profile;  // 1 bit 表示使用哪个级别的AAC
        unsigned int samplingFreqIndex;  // 4 bit 表示使用的采样频率索引
        unsigned int privateBit;  // 1 bit
        unsigned int channelCfg; // 3 bit
        unsigned int originalCopy;  // 1 bit
        unsigned int home;  // 1 bit

        /* 下面的参数每一帧都不同 */
        unsigned int copyrightIdentificationBit;  // 1 bit
        unsigned int copyrightIdentificationStart;  // 1 bit
        unsigned int aacFrameLength;  // 13 bit
        unsigned int adtsBufferFullness;  // 11 bit 0x7FF说明是码率可变的码流
        unsigned int numberOfRawDataBlockInFrame;  // 2 bit
    };

    bool parseAdtsHeader(uint8_t* in, struct AdtsHeader* res);
    int getFrameFromAACFile(uint8_t* buf, int size);

private:
    FILE* mFile;
    struct AdtsHeader mAdtsHeader;
};