#include "FFmpegDecoder.h"
#include "Common.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

#pragma warning(disable: 4996)

FFmpegDecoder::FFmpegDecoder() : mVideoCodecCtx(nullptr) {
    mFmtCtx = avformat_alloc_context();
}

FFmpegDecoder::~FFmpegDecoder() {
    if (mVideoCodecCtx) {
        avcodec_free_context(&mVideoCodecCtx);
        mVideoCodecCtx = nullptr;
    }
    if (mFmtCtx) {
        avformat_close_input(&mFmtCtx);
        avformat_free_context(mFmtCtx);
        mFmtCtx = nullptr;
    }
}

void FFmpegDecoder::testPullStream(int thread_num, const char* input, const char* rtsp_transport) {
    logw("thread-" + std::to_string(thread_num) + "], testPullStream start");

    AVDictionary* format_dict = nullptr;
    av_dict_set(&format_dict, "rtsp_transport", rtsp_transport, 0);
}