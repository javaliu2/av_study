#include "encode_yuv_to_h264.h"
#include "output_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
}

static void encode(AVCodecContext* pCodecCtx, AVFrame* pFrame, AVPacket* pPacket, FILE* p_output_f) {
    int ret;
    ret = avcodec_send_frame(pCodecCtx, pFrame);
    // 为什么需要while，一个frame会被编码为多个packet?
    while (ret >= 0) {
        ret = avcodec_receive_packet(pCodecCtx, pPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }
        // 1st: 准备写入文件的数据（数组形式）
        // 2nd: 每一个元素的大小（字节为单位）
        // 3th: 元素的个数
        // 4fo: 写入文件的句柄
        fwrite(pPacket->data, 1, pPacket->size, p_output_f);  // 每一个参数的含义是什么
        av_packet_unref(pPacket);
    }
}

int encode_yuv_to_h264(const char* output_filePath) {
    AVCodecContext* pCodecCtx = NULL;
    const AVCodec* pCodec = nullptr;
    AVPacket* pPacket = nullptr;
    AVFrame* pFrame = nullptr;
    char codec_name[] = "libx264";
    unsigned char endcode[]= {0x00, 0x00, 0x01, 0x7b};
    FILE* p_output_f = nullptr;
    int i, x, y;
    int ret = 0;

    pCodec = avcodec_find_encoder_by_name(codec_name);  // 查询指定名字的编码器
    if (!pCodec) {
        output_log(LOG_ERROR, "avcodec_find_encoder_by_name error, codec_name=%s", codec_name);
        ret = -1;
        goto end;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        output_log(LOG_ERROR, "avcodec_alloc_context3 error, pCodecCtx is NULL");
        ret = -1;
        goto end;
    }
    pPacket = av_packet_alloc();
    pFrame = av_frame_alloc();

    // set AVCodecContext parameters
    pCodecCtx->bit_rate = 400000;
    pCodecCtx->width = 352;
    pCodecCtx->height = 288;
    pCodecCtx->time_base = {1, 25};
    pCodecCtx->framerate = {25, 1};

    pCodecCtx->gop_size = 10;
    pCodecCtx->max_b_frames = 1;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    if (pCodec->id == AV_CODEC_ID_H264) {
        av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);
    }
    // open codec
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        output_log(LOG_ERROR, "avcodec_open2 error");
        ret = -1;
        goto end;
    }
    pFrame->format = pCodecCtx->pix_fmt;
    pFrame->width = pCodecCtx->width;
    pFrame->height = pCodecCtx->height;
    // allocate new buffer for audio or video data
    // 内存对齐。32表示什么
    if (av_frame_get_buffer(pFrame, 32) < 0) {
        output_log(LOG_ERROR, "av_frame_get_buffer error");
        ret = -1;
        goto end;
    }
    // open output_file
    fopen_s(&p_output_f, output_filePath, "wb");
    if (!p_output_f) {
        output_log(LOG_ERROR, "fopen_s error");
        ret = -1;
        goto end;
    }
    // encode 5 seconds of video
    // 帧率25，即1秒25张图片
    for (i = 0; i < 25 * 5; ++i) {
        fflush(stdout);
        // make sure the frame data is writable
        // buffer和frame之间的引用？buffer就是一块内存吧
        if (av_frame_is_writable(pFrame) < 0) {
            ret = -1;
            goto end;
        }
        // Y
        for (y = 0; y < pCodecCtx->height; ++y) {
            for (x = 0; x < pCodecCtx->width; ++x) {
                pFrame->data[0][y*pFrame->linesize[0]+x] = x + y + i * 3;
            }
        }
        // U and V
        for (y = 0; y < pCodecCtx->height / 2; ++y) {
            for (x = 0; x < pCodecCtx->width / 2; ++x) {
                pFrame->data[1][y*pFrame->linesize[1]+x] = 128 + y + i * 2;
                pFrame->data[2][y*pFrame->linesize[2]+x] = 64 + x + i * 5;
            }
        }
        pFrame->pts = i;
        // encode this image
        encode(pCodecCtx, pFrame, pPacket, p_output_f);
    }
    // flush the encoder
    encode(pCodecCtx, nullptr, pPacket, p_output_f);
    // add sequence end code to have a real MPEG file
    fwrite(endcode, 1, sizeof(endcode), p_output_f);
    fclose(p_output_f);
end:
    if (pCodecCtx) {
        avcodec_free_context(&pCodecCtx);
    } 
    if (pPacket) {
        av_packet_free(&pPacket);
    }
    if (pFrame) {
        av_frame_free(&pFrame);
    }
    printf("============= encode yuv to h264 done. =============\n");
    return ret;
}