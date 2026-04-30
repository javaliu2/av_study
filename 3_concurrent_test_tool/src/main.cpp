#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "Common.h"
using namespace std;

std::mutex mtx;
static void printf_tm_address() {
    lock_guard<std::mutex> lock(mtx);
    time_t v = time(nullptr);
    tm* t = localtime(&v);
    cout << "thread id: " << std::this_thread::get_id() << "; address of tm: " << t << endl;
}

std::mutex log_mtx;  // 必须定义在cpp文件中，而不能是函数内

int main() {
    std::cout << "getCurTimeStr() = " << getCurTimeStr() << std::endl;
    vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(
            std::thread([](){
                printf_tm_address();
            })
        );
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    /**
     * output:
     * thread id: thread id: 3; address of tm: 2; address of tm: 0x246153c2900
     * 0x246153c29b0
     * thread id: 4; address of tm: 0x246153c2960
     * 分析以上结果，输出的无序的，这是正常的，因为对临界区代码没有进行互斥访问。
     * 可以发现，3个线程输出的tm地址也是不同的，很可能用了“线程局部存储（TLS）实现”。
     * 复现失败，不管怎么，localtime是线程不安全的。
     */

    cout << getSteadyTimeMs() << endl;
    cout << getSystemTimeMs() << endl;

    
    string str = "hello, i am xs.";
    logw(str);
    return 0;
}
