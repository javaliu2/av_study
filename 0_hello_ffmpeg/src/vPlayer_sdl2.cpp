#define SDL_MAIN_HANDLED  // 我自己管理main
#include <stdio.h>
#include "output_log.h"
extern "C" {  // cpp文件中使用c头文件
    #include <SDL2/SDL.h>
    #include <libavcodec/avcodec.h>
}
int main() {
    printf("SDL version: %d.%d.%d\n",
           SDL_MAJOR_VERSION,
           SDL_MINOR_VERSION,
           SDL_PATCHLEVEL);

    printf("FFmpeg version: %s\n", av_version_info());
    output_log(LOG_DEBUG, "i am %s, and i am %d years old.", "xs", 26);
    return 0;
}
