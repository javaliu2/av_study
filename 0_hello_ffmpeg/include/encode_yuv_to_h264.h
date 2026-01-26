#pragma once

// 内部辅助函数（static封装）
// static修饰的函数表明该函数的作用域为.h对应的.cpp文件内，他不是API接口，所以不应该暴露给使用者，只在.cpp文件中定义就OK
// static void encode(AVCodecContext* pCodecCtx, AVFrame* pFrame, AVPacket* pPacket, FILE* p_output_f);

// 外部可访问的API
int encode_yuv_to_h264(const char* output_filePath);