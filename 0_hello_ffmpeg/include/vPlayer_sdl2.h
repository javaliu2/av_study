#ifndef VPLAYER_SDL_H
#define VPLAYER_SDL_H

#ifdef __cplusplus
extern "C" {
#endif
#include <SDL2/SDL.h>
#include <stdint.h>

/* =========================
 * forward declarations
 * ========================= */
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Thread;
struct SDL_Rect;

/* =========================
 * FFmpeg video params
 * ========================= */
typedef struct FFmpeg_V_Param_T {
    struct AVFormatContext* pFormatCtx;
    struct AVCodecContext*  pCodecCtx;
    struct SwsContext*      pSwsCtx;
    int                     video_index;
} FFmpeg_V_Param;

/* =========================
 * SDL params
 * ========================= */
typedef struct SDL_Param_T {
    struct SDL_Window*   p_sdl_window;
    struct SDL_Renderer* p_sdl_renderer;
    struct SDL_Texture*  p_sdl_texture;
    // 前向声明只能用于指针，因为指针变量的大小可以确定为8/16字节，而结构体的话，如果没有定义，不知道它的大小，就会报错“不完整类型”
    struct SDL_Rect      sdl_rect;
    struct SDL_Thread*   p_sdl_thread;
} SDL_Param;

/* =========================
 * FFmpeg interfaces
 * ========================= */
int init_ffmpeg(FFmpeg_V_Param* p_ffmpeg_param, const char* file_path);
int release_ffmpeg(FFmpeg_V_Param* p_ffmpeg_param);

/* =========================
 * SDL interfaces
 * ========================= */
int init_sdl(SDL_Param_T* p_sdl_param, int screen_w, int screen_h);
int release_sdl(SDL_Param_T* p_sdl_param);

/* =========================
 * player entry
 * ========================= */
int vPlayer_sdl(const char* file_path);

#ifdef __cplusplus
}
#endif

#endif // VPLAYER_SDL_H
