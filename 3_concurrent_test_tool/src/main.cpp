#include <vector>
#ifndef WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

#include <thread>
#include "Common.h"
#include "FFmpegDecoder.h"


std::mutex log_mtx;
std::mutex g_online_mtx;
int g_online_count;

void run(int type, int interval, int con_current, const char* input, const char* rtsp_transport, const char* video_decoder);
static void handleThread(int type, int thread_num, const char* input, const char* rtsp_transport, const char* video_decoder);

void run(int type, int interval, int con_current, const char* input, const char* rtsp_transport, const char* video_decoder) {
    std::vector<std::thread*> threads;
    for (int i = 0; i < con_current; ++i) {
        std::thread* t = new std::thread(handleThread, type, i, input, rtsp_transport, video_decoder);
        threads.push_back(t);
    }
    std::thread([&]() {
        while (true) {
            #ifndef WIN32
                usleep(interval * 1000);  // 该函数的休眠单位是微秒
            #else
                Sleep(interval);
            #endif
            g_online_mtx.lock();
            std::string log = "实时并发在线数:" + std::to_string(g_online_count);  // g_online_count是共享变量，所以需要使用互斥锁进行读写保护
            g_online_mtx.unlock();
            logw(log);
        }
    }).detach();
    for (int i = 0; i < threads.size(); ++i) {
        threads[i]->join();
    }
    for (int i = 0; i < threads.size(); ++i) {
        delete threads[i];
    }
    threads.clear();
}

/**
 * @param thread_num 线程编号
 */
static void handleThread(int type, int thread_num, const char* input, const char* rtsp_transport, const char* video_decoder) {
    g_online_mtx.lock();
    g_online_count++;
    g_online_mtx.unlock();

    FFmpegDecoder docoder;
    if (type == 0) {
        docoder.testPullStream(thread_num, input, rtsp_transport);
    } else if (type == 1) {
        docoder.testPullAndDecodeStream(thread_num, input, rtsp_transport, video_decoder);
    }
    g_online_mtx.lock();
    g_online_count--;
    g_online_mtx.unlock();
}

void printHelp() {
    printf("-h 查看帮助文档\n");
    printf("-t 测试类型(0: 拉流; 1: 拉流+解码), 如: -t 0\n");
    printf("-c 测试并发数, 如: -c 2\n");
    printf("-i 视频流地址(也可以是本地视频文件路径), 如: -i rtsp://127.0.0.1:554/live/test\n");
    printf("-v 视频流解码器, 如: h264, h264_qsv, h264_cuvid, hevc, hevc_qsv, hevc_cuvid\n");
    printf("-r rtsp拉流时传输层协议(udp或tcp), 如: -r udp\n");
    printf("-s 记录日志的间隔时间, 休眠单位为毫秒, 如: -s 1000\n");
}

int main(int argc, char* argv[]) {
    using namespace std;
    SetConsoleOutputCP(CP_UTF8);
    if (argc <= 1) {
        printHelp();
        system("pause\n");
        exit(0);
    }
    int type = 0;
    int con_current = 0;
    string input;
    string video_decoder;
    string rtsp_transport = "udp";
    int interval = 1000;

    // 第0个参数是可执行程序.exe，第1个参数之后是自定义参数
    /** 
     * PS C:\Users\23590\Documents\av_study\3_concurrent_test_tool\build> ."C:/Users/23590/Documents/av_study/3_concurrent_test_tool/build/main.exe" -name xs -age 26
     * 第0个参数, 值为C:\Users\23590\Documents\av_study\3_concurrent_test_tool\build\main.exe
     * 第1个参数, 值为-name
     * 第2个参数, 值为xs
     * 第3个参数, 值为-age
     * 第4个参数, 值为26
    */
    // for (int i = 0; i < argc; ++i) {
    //     printf("第%d个参数, 值为%s\n", i, argv[i]);
    // }

    for (int i = 1; i < argc; i += 2) {
        if (argv[i][0] != '-') {
            printf("输入参数有误: %s\n", argv[i]);
            return false;
        }
        switch (argv[i][1]) {
            case 'h':
                printHelp();
                system("pause");
                exit(0);
                return -1;
            case 't':
                type = atoi(argv[i+1]); break;
            case 'c':
                con_current = atoi(argv[i+1]); break;
            case 'i':
                input = argv[i+1]; break;
            case 'v':
                video_decoder = argv[i+1]; break;
            case 's':
                interval = atoi(argv[i+1]); break;
            case 'r':
                rtsp_transport = argv[i+1]; break;
            default: 
                printf("输入参数错误: %s\n", argv[i]);
                return -1;
        }
    }
    if ("tcp" != rtsp_transport && "udp" != rtsp_transport) {
        logw("未知的传输层协议");
        return 0;
    }
    if (interval < 1000 || interval > 1000000) {
        logw("记录日志的间隔时间, 休眠单位为毫秒, 取值范围[1000, 1000000]");
        return 0;
    }
    logw("测试类型:" + std::to_string(type));
    logw("测试并发数:" + std::to_string(con_current));
    logw("视频流地址:" + input);
    logw("视频流解码器:" + video_decoder);
    logw("记录日志休眠间隔:" + std::to_string(interval) + "毫秒");
    logw("rtsp传输层协议(视频流地址是rtsp协议的地址时有效):" + rtsp_transport);

    if (con_current > 0) {
        run(type, interval, con_current, input.data(), rtsp_transport.data(), video_decoder.data());
    }
    logw("程序结束");
    return 0;
}