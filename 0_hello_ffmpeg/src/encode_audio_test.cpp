#include <iostream>
#include "encode_pcm_to_mp2.h"
using namespace std;

int main() {
    const char* output_path = "../resource/encode_audio.mp2";
    encode_pcm_to_mp2(output_path);
    return 0;
}