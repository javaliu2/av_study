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
static int check_sample_fmt_old(const AVCodec* pCodec, enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = pCodec->sample_fmts;  // 获取一个enum类型AVSampleFormat变量的数组
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
static int check_sample_fmt(const AVCodec* pCodec, enum AVSampleFormat sample_fmt) {
    const AVSampleFormat *fmts = NULL;
    int nb_fmts = 0;
    int ret = 0;

    ret = avcodec_get_supported_config(NULL, pCodec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&fmts, &nb_fmts);
    if (ret < 0 || !fmts)
        return 0;
    for (int i = 0; i < nb_fmts; ++i) {
        if (fmts[i] == sample_fmt)
            return 1;
    }
    return 0;
}

/**
 * just pick the highest supported samplerate
 */
static int select_sample_rate_old(const AVCodec *pCodec) {
    const int *p;
    int best_samplerate = 0;
    if (!pCodec->supported_samplerates) {
        return 44100;  // 什么意思? 每秒钟采样44100次
    }
    p = pCodec->supported_samplerates;  //< array of supported audio samplerates, or NULL if unknown, array is terminated by 0
    while (*p) {
        // 从编码器支持的采样率中，选一个最接近 44100 的
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate)) {
            best_samplerate = *p;
        }
        ++p;
    }
    return best_samplerate;
}

static int select_sample_rate(const AVCodec *pCodec) {
    const int *rates = NULL;
    int nb_rates = 0;
    int best = 0;
    // const void **out_configs表明调用者被约束不能修改「FFmpeg 所拥有的数据本身」
    int ret = avcodec_get_supported_config(NULL, pCodec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void**)&rates, &nb_rates);
    if (ret < 0 || !rates || nb_rates == 0) {
        return 44100;
    }
    for (int i = 0; i < nb_rates; ++i) {
        int r = rates[i];
        if (!best || abs(44100 - r) < abs(44100 - best)) {
            best = r;
        }
    }
    return best;
}
/**
 * select layout with the highest channel count
 */
static AVChannelLayout select_channel_layout(const AVCodec *pCodec) {
    const AVChannelLayout *layouts = NULL;
    int nb_layouts = 0;
    AVChannelLayout best_layout = AV_CHANNEL_LAYOUT_MONO;
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
        output_log(LOG_ERROR, "avcodec_find_encoder error");
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
    pCodecCtx->ch_layout = select_channel_layout(pCodec);  // 新API版本下，channels在ch_layout结构体里
    // open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        ret = -1;
        goto end;
    }
    // set AVFrame parameters
    /** 
     * 1、编码器一次期望的 sample 数
     * 2、一个时间窗口（frame_size/sample_rate, 音频帧的本质: 固定采样点数的时间窗口
     * 3、一次 encode 的最小单位 */
    pFrame->nb_samples = pCodecCtx->frame_size;
    pFrame->format = pCodecCtx->sample_fmt;
    // pFrame->ch_layout = pCodecCtx->ch_layout;  // 可以直接这样赋值吗，不推荐这样写。因为AVChannelLayout不是POD(Plain Old Data)对象
    // 正确姿势
    av_channel_layout_copy(&pFrame->ch_layout, &pCodecCtx->ch_layout);
    if (av_frame_get_buffer(pFrame, 0) < 0) {
        output_log(LOG_ERROR, "av_frame_get_buffer error");
        ret = -1;
        goto end;
    }
    fopen_s(&p_output_f, output_filepath, "wb");
    if (!p_output_f) {
        output_log(LOG_ERROR, "fopen_s error");
        ret = -1;
        goto end;
    }
    output_log(LOG_DEBUG,
    "fmt=%s planar=%d ch=%d frame_size=%d",
    av_get_sample_fmt_name(pCodecCtx->sample_fmt),
    av_sample_fmt_is_planar(pCodecCtx->sample_fmt),
    pCodecCtx->ch_layout.nb_channels,
    pCodecCtx->frame_size);  // [Log-Debug]: fmt=s16 planar=0 ch=2 frame_size=1152, planar=0 ==> interleaved, namely packed
    // encode a single tone sound(单声源)
    t = 0;  // 当前正弦波的相位，单位弧度
    tincr = 2 * M_PI * 440.0 / pCodecCtx->sample_rate;  // t increment, 每个采样点相位要增加多少
    // 生成200帧音频，一帧包括frame_size个采样点（per channel）
    for (i = 0; i < 200; ++i) {
        if (av_frame_make_writable(pFrame) < 0) {
            output_log(LOG_ERROR, "av_frame_make_writable error");
            ret = -1;
            goto end;
        }
        int ch = pCodecCtx->ch_layout.nb_channels;
        samples = (uint16_t*)pFrame->data[0];
        // 因为是packed，所以采用这种写法。planar就不是了
        for (j = 0; j < pCodecCtx->frame_size; ++j) {  // j: 这一帧中的第j个采样点
            samples[ch * j] = (int)(sin(t) * 10000);  // 采样
            for (k = 1; k < ch; ++k) {  // k: 声道索引
                samples[ch * j + k] = samples[ch * j];  // 其他声道共用一个采样值
            }
            t += tincr;
        }
        encode(pCodecCtx, pFrame, pPacket, p_output_f);
    }
    // flush the encoder
    encode(pCodecCtx, NULL, pPacket, p_output_f);
    fclose(p_output_f);
end:
    if (pCodecCtx) 
        avcodec_free_context(&pCodecCtx);
    if (pPacket)
        av_packet_free(&pPacket);
    if (pFrame) {
        av_frame_free(&pFrame);
    }
    return ret;
}