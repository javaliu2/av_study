#pragma once
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#endif

// 设置颜色（跨平台）
static inline void set_log_color_info() {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_GREEN | FOREGROUND_INTENSITY);
#else
    printf("\033[32m");  // 绿色
#endif
}

static inline void set_log_color_debug() {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // 青色
#else
    printf("\033[36m");  // 青色
#endif
}

static inline void set_log_color_error() {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_INTENSITY);
#else
    printf("\033[31m");  // 红色
#endif
}

static inline void reset_log_color() {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    printf("\033[0m");
#endif
}

// 日志宏
#define LOG_INFO(fmt, ...)  do { \
    set_log_color_info(); \
    printf("[INFO ][%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    reset_log_color(); \
} while (0)

#define LOG_DEBUG(fmt, ...) do { \
    set_log_color_debug(); \
    printf("[DEBUG][%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    reset_log_color(); \
} while (0)

#define LOG_ERROR(fmt, ...) do { \
    set_log_color_error(); \
    printf("[ERROR][%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    reset_log_color(); \
} while (0)
