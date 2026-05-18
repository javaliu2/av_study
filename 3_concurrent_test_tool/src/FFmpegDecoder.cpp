#include "FFmpegDecoder.h"
#include "Common.h"
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <thread>
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

void FFmpegDecoder::testPullStream(int thread_num, const char* input, const char* rtsp_transport, const char* decoder) {
    logw("thread-" + std::to_string(thread_num) + "], testPullStream start");

    AVDictionary* format_dict = nullptr;
    // 如果format_dict为空，那么av_dict_set方法会创建该对象，这和面向对象中创建对象的方式不太一致
    av_dict_set(&format_dict, "rtsp_transport", rtsp_transport, 0);  // 设置rtsp底层网络协议 tcp or udp
    av_dict_set(&format_dict, "stimeout", "3000000", 0);  // 设置rtsp连接超时（单位us, 10^6 us = 1s）
    av_dict_set(&format_dict, "rw_timeout", "3000000", 0);  // 设置rtmp/http-flv连接超时
    if (avformat_open_input(&mFmtCtx, input, nullptr, &format_dict) != 0) {
        logw("avformat_open_input error:" + std::string(input));
        return;
    }
    if (avformat_find_stream_info(mFmtCtx, nullptr) < 0) {
        logw("avformat_find_stream_info error:" + std::string(input));
        return;
    }

    // video start
    int srcVideoIndex = av_find_best_stream(mFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

    int curVideoWidth = -1;
    int curVideoHeight = -1;
    int curVideoFps = -1;
    int curVideoChannels = 3;
    if (srcVideoIndex > -1) {

    } else {
        logw("av_find_best_stream error:" + std::string(input));
        return;
    }
    // video end
    AVBufferRef* device_ref = nullptr;
    int ret = av_hwdevice_ctx_create(&device_ref, AV_HWDEVICE_TYPE_CUDA, "auto", nullptr, 0);
    if (ret < 0) {
        logw("av_hwdevice_ctx_create error:" + std::string(input));
        return;
    }
    AVCodecParameters* videoCodecPar = mFmtCtx->streams[srcVideoIndex]->codecpar;
    const AVCodec* videoCodec = nullptr;
    videoCodec = avcodec_find_decoder_by_name(decoder);
    if (videoCodecPar->codec_id == AV_CODEC_ID_H264) {
        // decoders: h264 h264_qsv h264_cuvid
    } else if (videoCodecPar->codec_id == AV_CODEC_ID_HEVC) {
        // decoders: hevc hevc_qsv hevc_cuvid
    }

    if (videoCodec) {
        logw("hardware decode h264 success:" + std::string(decoder));
    } else {
        logw("hardware decode h264 error:" + std::string(decoder));
        return;
    }

    if (!videoCodec) {
        videoCodec = avcodec_find_decoder(videoCodecPar->codec_id);
        if (!videoCodec) {
            logw("hardware cpu decode h264 error");
            return;
        }
    }

    mVideoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (avcodec_parameters_to_context(mVideoCodecCtx, videoCodecPar) != 0) {
        logw("avcodec_parameters_to_context error:" + std::string(input));
        return;
    }

    if (avcodec_open2(mVideoCodecCtx, videoCodec, nullptr) < 0) {
        logw("avcodec_open2 error:" + std::string(input));
        return;
    }

    curVideoWidth = mVideoCodecCtx->width;
    curVideoHeight = mVideoCodecCtx->height;
    mVideoStream = mFmtCtx->streams[srcVideoIndex];

    if (0 == mVideoStream->avg_frame_rate.den) {
        char log[300];
        sprintf(log, "url=%s,index=%d,avg_frame_rate(num=%d,den=%d),r_frame_rate(num=%d,den=%d)",
        input, srcVideoIndex, mVideoStream->avg_frame_rate.num, mVideoStream->avg_frame_rate.den,
        mVideoStream->r_frame_rate.num, mVideoStream->r_frame_rate.den);
        logw(log);
        curVideoFps = 25;
    } else {
        curVideoFps = mVideoStream->avg_frame_rate.num / mVideoStream->avg_frame_rate.den;
    }

    char log[300];
    sprintf(log, "url=%s,videoIndex=%d,videoWidth=%d,videoHeight=%d,videoFps=%d",
        input, srcVideoIndex, curVideoWidth, curVideoHeight, curVideoFps);
    logw(log);

    // 开始解码
    mSrcVideoWidth = curVideoWidth;
    mSrcVideoHeight = curVideoHeight;
    int srcVideoChannels = curVideoChannels;
    int srcVideoFps = curVideoFps;

    AVPacket pkt;  // 未解码的视频帧
    // Allocate an AVFrame and set its fields to default values. The resulting struct must be freed using av_frame_free().
    // 分配一个AVFrame结构体对象并且设置他的字段为默认值，需要使用av_frame_free()释放该结构体对象
    AVFrame* frame_yuv420p = av_frame_alloc();
    AVFrame* frame_bgr = av_frame_alloc();
    
    // Return the size in bytes of the amount of data required to store an image with the given parameters.
    // 返回存储给定参数的图片数据的大小，以字节为单位
    int frame_bgr_buff_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, mSrcVideoWidth, mSrcVideoHeight, 1);
    // Allocate a memory block with alignment suitable for all memory accesses (including vectors if available on the CPU).
    // 对于所有的内存访问，分配合适对齐的内存块。（如果在CPU上可用的话，包括向量）
    uint8_t* frame_bgr_buff = (uint8_t*)av_malloc(frame_bgr_buff_size);
    av_image_fill_arrays(frame_bgr->data, frame_bgr->linesize, frame_bgr_buff, AV_PIX_FMT_BGR24, mSrcVideoWidth, mSrcVideoHeight, 1);
    
    // Allocate and return an SwsContext. You need it to perform scaling/conversion operations using sws_scale().
    // 分配然后返回一个SwsContext，使用sws_scale()执行缩放和转换操作。
    SwsContext* sws_ctx_yuv420p2bgr = sws_getContext(
        mSrcVideoWidth, mSrcVideoHeight,
        mVideoCodecCtx->pix_fmt,
        mSrcVideoWidth, mSrcVideoHeight, AV_PIX_FMT_BGR24,
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    
        int64_t frameCount = 0;
        while (true) {
            if (av_read_frame(mFmtCtx, &pkt) >= 0) {
                if (pkt.stream_index == srcVideoIndex) {
                    ret = avcodec_send_packet(mVideoCodecCtx, &pkt);
                    if (ret == 0) {
                        ret = avcodec_receive_frame(mVideoCodecCtx, frame_yuv420p);
                        if (ret == 0) {
                            frameCount++;
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        } else {
                            logw("avcodec_receive_frame error: ret=" + std::to_string(ret));
                        }
                    } else {
                        char log[200];
                        sprintf(log, "avcodec_send_packet error: ret=%d", ret);
                        logw(log);
                    }
                }
                // 获取的pkt，必须释放
                av_packet_unref(&pkt);
            } else {
                break;
            }
        }
        av_frame_free(&frame_yuv420p);
        frame_yuv420p = nullptr;
        av_frame_free(&frame_bgr);
        frame_bgr = nullptr;

        av_free(frame_bgr_buff);
        frame_bgr_buff = nullptr;

        sws_freeContext(sws_ctx_yuv420p2bgr);
        sws_ctx_yuv420p2bgr = nullptr;

        logw("[thread-" + std::to_string(thread_num) + "],testPullDecodeStream end");
}

