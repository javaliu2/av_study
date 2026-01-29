#include "output_log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/common.h>
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
}
#define DEBUG_INFO 1
/**
 * check that a given sample format is supported by the encoder
 */
static int check_sample_fmt(const AVCodec* pCodec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = pCodec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        #ifdef DEBUG_INFO
        output_log(LOG_DEBUG, "*p= %d", *p);
        #endif
        if (*p == sample_fmt) {
            return 1;
        }
        ++p;  // 他会循环加回来？比如enum中有10个元素，加到9，下面会加到0？
    }
    return 0;
}

/**
 * just pick the highest supported samplerate
 */
static int select_sample_rate(const AVCodec *pCodec) {
    const int *p;
    int best_samplerate = 0;
    if (!pCodec->supported_samplerates) {
        return 44100;  // 什么意思? 每秒钟采样44100次
    }
    p = pCodec->supported_samplerates;  //< array of supported audio samplerates, or NULL if unknown, array is terminated by 0
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate)) {
            best_samplerate = *p;
        }
        ++p;
    }
    return best_samplerate;
}

/**
 * select layout with the highest channel count
 */
static AVChannelLayout select_channel_layout(const AVCodec *pCodec) {
    const AVChannelLayout *layouts = NULL;
    int nb_layouts = 0;
    AVChannelLayout best_layout;
    int best_nb_channels = 0;
    avcodec_get_supported_config(NULL, pCodec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, (const void**)&layouts, &nb_layouts);
    if (nb_layouts <= 0) {
        av_channel_layout_default(&best_layout, 2);
        return best_layout;  // 如果编码器没有指定支持的声道布局，就默认使用立体声
    }
    for (int i = 0; i < nb_layouts; ++i) {
        if (layouts[i].nb_channels > best_nb_channels) {
            best_nb_channels = layouts[i].nb_channels;
            av_channel_layout_copy(&best_layout, &layouts[i]);
        }
    }
    return best_layout;
}

static void encode(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVPacket *pPacket, FILE *p_output_f) {
    int ret = 0;
    ret = avcodec_send_frame(pCodecCtx, pFrame);
    while (ret >= 0) {
        ret = avcodec_receive_packet(pCodecCtx, pPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        fwrite(pPacket->data, 1, pPacket->size, p_output_f);
        av_packet_unref(pPacket);
    }
}

int encode_pcm_to_mp2(const char* output_filepath) {
    AVCodecContext* pCodecCtx = NULL;
    const AVCodec* pCodec = NULL;
    AVPacket* pPacket = NULL;
    AVFrame* pFrame = NULL;
    FILE* p_output_f = NULL;
    int i, j, k, ret = 0;
    uint16_t *samples;
    float t, tincr;
    pCodec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!pCodec)  {
        output_log(LOG_ERROR, "avcodec_find_encoder(AV_CODEC_ID_MP2) error");
        ret = -1;
        goto end; 
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);  
    pPacket = av_packet_alloc(); 
    if (!pPacket) {    
        output_log(LOG_ERROR, "av_packet_alloc error");   
        ret = -1;  
        goto end;  
    }  
    pFrame = av_frame_alloc();  
    if (!pFrame) {    
        output_log(LOG_ERROR, "av_frame_alloc error");  
        ret = -1; 
        goto end; 
    }
    // set AVCodecContext parameters
    pCodecCtx->bit_rate = 64000;
    /* check that the encoder supports s16 pcm input */
    pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(pCodec, pCodecCtx->sample_fmt)) {
        output_log(LOG_ERROR, "check_sample_fmt error");  
        ret = -1; 
        goto end; 
    }
    /* select other audio parameter supported by the encoder */
    pCodecCtx->sample_rate = select_sample_rate(pCodec);
    pCodecCtx->ch_layout = select_channel_layout(pCodec);
end:
    return 0;
}