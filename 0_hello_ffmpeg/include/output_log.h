#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别 */
typedef enum LOG_LEVEL {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LOG_LEVEL;

/* 设置各级别日志开关 */
void set_log_flag(int log_debug_flag,
                  int log_info_flag,
                  int log_warnning_flag,
                  int log_error_flag);

/* 日志输出函数（可变参数，printf 风格） */
void output_log(LOG_LEVEL log_level, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // LOG_H
