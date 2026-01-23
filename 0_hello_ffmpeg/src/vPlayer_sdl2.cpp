// #define SDL_MAIN_HANDLED  // 我自己管理main
#include <stdio.h>
#include <iostream>
#include "output_log.h"
#include "vPlayer_sdl2.h"
#define __STDC_CONSTANT_MACROS 
extern "C" {  // cpp文件中使用c头文件
    #include <SDL2/SDL.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}
using namespace std;

static int g_frame_rate = 1;  // static表示该变量只对当前cpp可见，其他cpp文件不可用
static int g_sfp_refresh_thread_exit = 0;
static int g_sfp_refresh_thread_pause = 0;

#define SFM_REFRESH_EVENT (SDL_USEREVENT+1)
#define SFM_BREAK_EVENT (SDL_USEREVENT+2)

// typedef struct FFmpeg_V_Param_T {
//     AVFormatContext* pFormatCtx;
//     AVCodecContext* pCodecCtx;  // codec: COder(编码器) + DECoder(解码器), 编解码器
//     SwsContext* pSwsCtx;
//     int video_index;
// } FFmpeg_V_Param;

// typedef struct SDL_Param_T {
//     SDL_Window* p_sdl_window;
//     SDL_Renderer* p_sdl_renderer;
//     SDL_Texture* p_sdl_texture;
//     SDL_Rect sdl_rect;
//     SDL_Thread* p_sdl_thread;
// } SDL_Param;

int init_ffmpeg(FFmpeg_V_Param* p_ffmpeg_param, const char* file_path) {
    p_ffmpeg_param->pFormatCtx = avformat_alloc_context();
    const AVCodec* pCodec = NULL;
    // pCodec = (AVCodec*)"sss";  // ok, 可以修改指针的值
    // pCodec->name = "fff";  // 报错: expression must be a modifiable lvalue，不能通过指针修改其所指向的对象
    avformat_network_init();  // 进行全局网络初始化工作
    // ->优先级高于&, 所以&p_ffmpeg_param->pFormatCtx <==> &(p_ffmpeg_param->pFormatCtx)
    // 传递二级指针，方便avformat_open_input修改init_ffmpeg中pFormatCtx指针变量的值
    // open input stream
    if (avformat_open_input(&p_ffmpeg_param->pFormatCtx, file_path, nullptr, nullptr) != 0) {
        output_log(LOG_ERROR, "avformat_open_input error");
        return -1;
    }
    // find stream info
    if (avformat_find_stream_info(p_ffmpeg_param->pFormatCtx, nullptr) < 0) {
        output_log(LOG_ERROR, "avformat_find_stream_info error");
        return -1;
    }
    // get video pCodecParams, codec and frame rate
    output_log(LOG_DEBUG, "nb_streams: %d", p_ffmpeg_param->pFormatCtx->nb_streams);  // [Log-Debug]: nb_streams: 1, 表示只有video
    // nb_streams是说该音视频文件里面有几个流，以下for循环不是对某一个流中所有数据的遍历，而是对流的遍历
    for (int i = 0; i < p_ffmpeg_param->pFormatCtx->nb_streams; ++i) {
        AVStream* pStream = p_ffmpeg_param->pFormatCtx->streams[i];
        if (pStream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
           output_log(LOG_DEBUG, "codec_id: %d", pStream->codecpar->codec_id);  // [Log-Debug]:codec_id: 1, 对应AV_CODEC_ID_MPEG1VIDEO
           pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);  // 编解码器
           p_ffmpeg_param->pCodecCtx = avcodec_alloc_context3(pCodec);
           avcodec_parameters_to_context(p_ffmpeg_param->pCodecCtx, pStream->codecpar);
           g_frame_rate = pStream->avg_frame_rate.num / pStream->avg_frame_rate.den;  // 分子 / 分母
           output_log(LOG_DEBUG, "g_frame_rate: %d", g_frame_rate);  // [Log-Debug]: g_frame_rate: 25
           p_ffmpeg_param->video_index = i;  // 0
           pStream->time_base;  // 时间基单位
        }
    }
    if (!p_ffmpeg_param->pCodecCtx) {
        output_log(LOG_ERROR, "could not find video codecCtx");
        return -1;
    }
    // open codec
    if (avcodec_open2(p_ffmpeg_param->pCodecCtx, pCodec, nullptr)) {
        output_log(LOG_ERROR, "avcodec_open2 error");
        return -1;
    }
    // get scale pixelformat context
    p_ffmpeg_param->pSwsCtx = sws_getContext(p_ffmpeg_param->pCodecCtx->width,
    p_ffmpeg_param->pCodecCtx->height, p_ffmpeg_param->pCodecCtx->pix_fmt,
    p_ffmpeg_param->pCodecCtx->width, p_ffmpeg_param->pCodecCtx->height,
    AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);  // 拿到SwsContext对象，具体的转化需要调用sws_scale()

    av_dump_format(p_ffmpeg_param->pFormatCtx, p_ffmpeg_param->video_index, file_path, 0);
    /**
     * 以上函数的输出：
     * Input #0, mpeg, from '../resource/test.mpg':
     * Duration: 00:00:09.94, start: 0.540000, bitrate: 1045 kb/s
     * Stream #0:0[0x1e0]: Video: mpeg1video, yuv420p(tv, progressive), 1280x720 [SAR 1:1 DAR 16:9], 104857 kb/s, 25 fps, 50 tbr, 90k tbn, start 0.540000
     * Side data:
         cpb: bitrate max/min/avg: 0/0/0 buffer size: 49152 vbv_delay: N/A
     */
    return 0;
}

int release_ffmpeg(FFmpeg_V_Param* p_ffmpeg_param) {
    if (!p_ffmpeg_param) {
        return -1;
    }
    // release scale pixel format context
    if (p_ffmpeg_param->pSwsCtx) {
        sws_freeContext(p_ffmpeg_param->pSwsCtx);
    }
    // close codec
    if (p_ffmpeg_param->pCodecCtx) {
        // avcodec_close(p_ffmpeg_param->pCodecCtx);  // 旧API
        avcodec_free_context(&p_ffmpeg_param->pCodecCtx);  // close + free
    }
    if (p_ffmpeg_param->pFormatCtx) {
        avformat_close_input(&p_ffmpeg_param->pFormatCtx);
        avformat_free_context(p_ffmpeg_param->pFormatCtx);
    }
    // free FFmpeg_V_Param
    delete p_ffmpeg_param;
    p_ffmpeg_param = NULL;

    return 0;
}
// sfp是作者起的名字，可能是simple ffmpeg player OR sdl + ffmpeg player
// opaque本意是不透明的，黑盒的。在API设计中的意思是“我不关心你里面是什么，只负责原样传回去”
int sfp_refresh_thread(void* opaque) {
    g_sfp_refresh_thread_exit = 0;
    g_sfp_refresh_thread_pause = 0;
    while (!g_sfp_refresh_thread_exit) {
        if (!g_sfp_refresh_thread_pause) {
            SDL_Event sdl_event;
            sdl_event.type = SFM_REFRESH_EVENT;  // SFM是作者自定义前缀，+1表示这是自定义的第一个事件
            SDL_PushEvent(&sdl_event);
        }
        SDL_Delay(1000 / g_frame_rate);  // Wait a specified number of milliseconds before returning.
        // 1000 / 25 = 40ms=0.04s, 即程序在这里阻塞0.04秒，这刚好是一帧显示的时间长短
        // G-bro:
        // 按视频帧率控制刷新节奏：
        // 例如 25fps → 1000 / 25 = 40ms ≈ 0.04s
        // 即每隔一帧的理想显示时间，向主线程发送一次“刷新一帧”的事件
    }
    g_sfp_refresh_thread_exit = 0;
    g_sfp_refresh_thread_pause = 0;
    SDL_Event sdl_event;
    sdl_event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&sdl_event);
    return 0;
}

int init_sdl(SDL_Param_T* p_sdl_param, int screen_w, int screen_h) {
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
        output_log(LOG_ERROR, "SDL_Init error");
        return -1;
    }
    p_sdl_param->p_sdl_window = SDL_CreateWindow("vPlayer_sdl", SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL);
    if (!p_sdl_param->p_sdl_window) {
        output_log(LOG_ERROR, "SDL_CreateWindow error");
        return -1;
    }
    p_sdl_param->p_sdl_renderer = SDL_CreateRenderer(p_sdl_param->p_sdl_window, -1, 0);
    // texture是存在于 GPU（或渲染后端）中的像素数据
    p_sdl_param->p_sdl_texture = SDL_CreateTexture(p_sdl_param->p_sdl_renderer, SDL_PIXELFORMAT_IYUV, 
                                                  SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);
    p_sdl_param->sdl_rect.x = 0;
    p_sdl_param->sdl_rect.y = 0;
    p_sdl_param->sdl_rect.w = screen_w;
    p_sdl_param->sdl_rect.h = screen_h;
    // 启动一个新线程，在线程中执行 sfp_refresh_thread, 线程之间通过 SDL 事件队列进行通信
    p_sdl_param->p_sdl_thread = SDL_CreateThread(sfp_refresh_thread, NULL, NULL);
    return 0;
}
int release_sdl(SDL_Param_T* p_sdl_param) {
    SDL_DestroyTexture(p_sdl_param->p_sdl_texture);
    SDL_DestroyRenderer(p_sdl_param->p_sdl_renderer);
    SDL_DestroyWindow(p_sdl_param->p_sdl_window);
    SDL_Quit();
    return 0;
}
int vPlayer_sdl(const char* file_path) {
    // ffmpeg param
    FFmpeg_V_Param *p_ffmpeg_param = NULL;
    AVPacket * packet = NULL;  // 包含编码的数据
    // 解码之后的数据放到pFrame,但是图像格式可能为rgb或YUV444,借助sws_context将其转换为YUV420,存储到pFrameYUV供SDL显示使用
    AVFrame *pFrame = NULL, *pFrameYUV = NULL;
    int out_buffer_size = 0;
    unsigned char* out_buffer = 0;

    // sdl param
    SDL_Param_T *p_sdl_param = NULL;
    SDL_Event sdl_event;

    int ret = 0;
    // init ffmpeg
    p_ffmpeg_param = new FFmpeg_V_Param();
    memset(p_ffmpeg_param, 0, sizeof(FFmpeg_V_Param));
    if (init_ffmpeg(p_ffmpeg_param, file_path)) {
        ret = -1;
        goto end;  // 释放资源
    }
    packet = av_packet_alloc();
    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, p_ffmpeg_param->pCodecCtx->width, p_ffmpeg_param->pCodecCtx->height, 1);
    output_log(LOG_DEBUG, "out_buffer_size: %d", out_buffer_size);  // [Log-Debug]: out_buffer_size: 1382400 = 1280*720*1.5(YUV420中一个像素平均1.5字节)
    out_buffer = (unsigned char*)av_malloc(out_buffer_size);
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
    p_ffmpeg_param->pCodecCtx->pix_fmt, p_ffmpeg_param->pCodecCtx->width, p_ffmpeg_param->pCodecCtx->height, 1);
    // init sdl
    p_sdl_param = new SDL_Param_T();
    memset(p_sdl_param, 0, sizeof(SDL_Param_T));  // attention, it should be struct name, not the variable name
    if (init_sdl(p_sdl_param, p_ffmpeg_param->pCodecCtx->width, p_ffmpeg_param->pCodecCtx->height)) {
        ret = -1;
        goto end;
    }
    // demuxing and show
    while (true) {
        int temp_ret = 0;
        SDL_WaitEvent(&sdl_event);
        if (sdl_event.type == SFM_REFRESH_EVENT) {
            // 1、解码一帧
            while (true) {
                if (av_read_frame(p_ffmpeg_param->pFormatCtx, packet) < 0) {
                    g_sfp_refresh_thread_exit = 1;
                    break;
                }
                if (packet->stream_index == p_ffmpeg_param->video_index) {
                    break;
                }
            }
            if (avcodec_send_packet(p_ffmpeg_param->pCodecCtx, packet)) {
                g_sfp_refresh_thread_exit = 1;
            }
            do {
                temp_ret = avcodec_receive_frame(p_ffmpeg_param->pCodecCtx, pFrame);
                if (temp_ret == AVERROR_EOF) {
                    g_sfp_refresh_thread_exit = 1;
                    break;
                }
                if (temp_ret == 0) {
                    // 2、转格式
                    // sws: SoftWare Scaling
                    // param hint: const uint8_t *const srcSlice[]: srcSlice是一个指针数组，其中的指针数据不能被修改(const uint8_t *)，
                    // 另外srcSlice本身值也不可被修改，srcSlice是一个数组，其实就是一个指针
                    sws_scale(p_ffmpeg_param->pSwsCtx, (const unsigned char* const*)pFrame->data, 
                pFrame->linesize, 0, p_ffmpeg_param->pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
                    // 3、渲染一帧
                    SDL_UpdateTexture(p_sdl_param->p_sdl_texture, &p_sdl_param->sdl_rect, pFrameYUV->data[0],
                    pFrameYUV->linesize[0]);
                    SDL_RenderClear(p_sdl_param->p_sdl_renderer);
                    SDL_RenderCopy(p_sdl_param->p_sdl_renderer, p_sdl_param->p_sdl_texture,
                    NULL, &p_sdl_param->sdl_rect);
                    SDL_RenderPresent(p_sdl_param->p_sdl_renderer);
                }
            } while (temp_ret != AVERROR(EAGAIN));
        } else if (sdl_event.type == SFM_BREAK_EVENT) {
            break;
        } else if (sdl_event.type == SDL_KEYDOWN) {
            if (sdl_event.key.keysym.sym == SDLK_SPACE) {
                g_sfp_refresh_thread_pause = !g_sfp_refresh_thread_pause;
            }
            if (sdl_event.key.keysym.sym == SDLK_q) {
                g_sfp_refresh_thread_exit = 1;
            }
        } else if (sdl_event.type == SDL_QUIT) {
            g_sfp_refresh_thread_exit = 1;
        }
    }
    end:
        release_ffmpeg(p_ffmpeg_param);
        av_packet_free(&packet);
        av_frame_free(&pFrame);
        av_frame_free(&pFrameYUV);
        release_sdl(p_sdl_param);
    return ret;
}

// int SDL_main() {
//     printf("SDL version: %d.%d.%d\n",
//            SDL_MAJOR_VERSION,
//            SDL_MINOR_VERSION,
//            SDL_PATCHLEVEL);

//     printf("FFmpeg version: %s\n", av_version_info());
//     output_log(LOG_DEBUG, "i am %s, and i am %d years old.", "xs", 26);
//     return 0;
// }
