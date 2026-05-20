#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class FFmpegDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder();
public:
    // 测试拉流
    void testPullStream(int thread_num, const char* input, const char* rtsp_transport);
    // 测试拉流+解码
    void testPullAndDecodeStream(int thread_num, const char* input, const char* rtsp_transport, const char* decoder);
private:
    AVFormatContext* mFmtCtx;
    AVCodecContext* mVideoCodecCtx;
    AVStream* mVideoStream;
    int mSrcVideoWidth;
    int mSrcVideoHeight;
};
