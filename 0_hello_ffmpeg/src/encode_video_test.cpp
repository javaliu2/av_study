#include <iostream>
#include "encode_yuv_to_h264.h"
using namespace std;

int main() {
    const char* output_path = "../resource/352_multi_288_yuv420p.h264";
    encode_yuv_to_h264(output_path);
    return 0;
}