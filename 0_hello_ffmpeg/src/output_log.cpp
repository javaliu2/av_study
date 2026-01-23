#include <stdio.h>
#include <stdarg.h>
#include "output_log.h"
#define MAX_BUF_LEN 1024
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_PURPLE  "\033[35m"

int g_log_debug_flag = 1;
int g_log_info_flag = 1;
int g_log_warnning_flag = 1;
int g_log_error_flag = 1;

void set_log_flag(int log_debug_flag, int log_info_flag, int log_warnning_flag, int log_error_flag) {
    g_log_debug_flag = log_debug_flag;
    g_log_info_flag = log_info_flag;
    g_log_warnning_flag = log_warnning_flag;
    g_log_error_flag = log_error_flag;
}

void output_log(LOG_LEVEL log_level, const char* fmt, ...) {
    va_list args;  // variable parameter，可变参数
    va_start(args, fmt);

    char buf[MAX_BUF_LEN] = {0};
    vsnprintf(buf, MAX_BUF_LEN-1, fmt, args);  // 将args按照fmt格式组装成字符串置于buf中

    switch (log_level)
    {
    case LOG_DEBUG:
        if (g_log_debug_flag)
            printf(COLOR_BLUE "[Log-Debug]: %s" COLOR_RESET "\n", buf);
        break;
    case LOG_INFO:
        if (g_log_info_flag)
            printf(COLOR_GREEN "[Log-Info]: %s" COLOR_RESET "\n", buf);
        break;
    case LOG_WARNING:
        if (g_log_warnning_flag)
        printf(COLOR_YELLOW "[Log-Warnning]: %s" COLOR_RESET "\n", buf);
        break;
    case LOG_ERROR:
        if (g_log_error_flag)
        printf(COLOR_RED "[Log-Error]: %s" COLOR_RESET "\n", buf);  // 相邻的字符串字面量在编译期会自动合并
        break;
    default:
        // char* c = "aaa" "bb" "cc";  // it's ok
        break;
    }
    va_end(args);
}