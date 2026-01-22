#include <iostream>
#include "vPlayer_sdl2.h"
#include <SDL2/SDL.h>
#define SDL_MAIN_HANDLED
int main(int argc, char* argv[]) {
    char* file_path = "../resource/test.mpg";
    vPlayer_sdl(file_path);
    return 0;
}
