#pragma once

#include <time.h>
#include <vector>
#include <chrono>
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

#pragma warning(disable: 4996) // disable warning for unsafe functions like localtime

static std::string getCurTimeStr() {
    const char* time_fmt = "%Y-%m-%d %H:%M:%S";
    // time() returns the current calendar time as a time_t value and, 
    // if _Time is non-null, also stores that value through the provided pointer.
    // 返回自1970-01-01 00:00:00 UTC以来的秒数
    time_t now = time(nullptr);
    printf("time() result: %lld\n", static_cast<long long>(now));
    char time_str[64];
    // localtime() Converts a time_t value to a tm structure representing the local calendar time, 
    // returning a pointer to a statically allocated tm object.
    size_t res = strftime(time_str, sizeof(time_str), time_fmt, localtime(&now));
    // 以上函数的含义即是str format time，将time格式化为指定字符串形式的时间表示
    printf("strftime result: %d\n", res);  // 结果是19，因为是写入time_str的字符个数，而不是time_fmt的长度
    return std::string(time_str);  // 2026-04-30 15:11:39
    // 64位int(有符号位)可以表示的年数=(2^63-1)/（365*24*60*60) ~= 2924,7120,8677, 2924亿年之多
}

static int64_t getSteadyTimeMs() {
    // 1、获取系统当前时间点
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    // 2、获取从时钟纪元到当前时间点的时长
    // 针对steady_clock，该时钟纪元，通常是系统启动时间，或者某个内部时间
    std::chrono::steady_clock::duration duration_since_epoch = now.time_since_epoch();
    // 3、将时长转换位特定单位
    auto millisecond_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch);
    // 4、获取数值
    long long timestamp = millisecond_since_epoch.count();
    return timestamp;
}

static int64_t getSystemTimeMs() {
    // 1、获取系统当前时间点
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    // 2、获取从时钟纪元到当前时间点的时长
    // 针对sytem_clock，时钟纪元，为1970-01-01 00:00:00 UTC
    std::chrono::system_clock::duration duration_since_epoch = now.time_since_epoch();
    auto millisecond_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch);
    long long timestamp = millisecond_since_epoch.count();
    return timestamp;
}

extern std::mutex log_mtx;  // 声明，未定义。必须在某一个cpp文件中定义，必须定义且只能定义一次。否则在链接阶段会报错未找到定义或者重复定义的问题
static void logw(std::string log) {
    std::lock_guard<std::mutex> lock(log_mtx);
    std::ofstream fs;
    // std::ios_base::app is a static constant openmode flag that opens a file in append mode, 
    // meaning output is positioned at the end of the file before each write.
    fs.open("log.log", std::ofstream::app);
    fs << "[W]" << getCurTimeStr() << " " << log << std::endl;  // 写到文件
    std::cout << "'[W]" << getCurTimeStr() << " " << log << std::endl;  // 写到console
    fs.close();
}