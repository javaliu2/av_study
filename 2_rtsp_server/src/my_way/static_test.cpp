#include <iostream>
#include "static_test.h"

using namespace std;
/**
 * vscode tips: a storage class may not be specified here
 * 对以上错误的解释：这里不允许再写“存储类型说明符（storage class specifier）”，而 static 就属于这个类别。
 * 
 * 类成员函数在声明时，已经“绑定到类”了
 * .h: static void Socket::getPeerIp(...);
 * 这一刻，这个函数已经是：一个类成员函数; 具有外部链接属性（由类定义决定）
 * 👉 编译器已经知道它是谁了
 * ❌ 你再加 static 会发生冲突
 * .cpp: static void Socket::getPeerIp(...){}
 * 这里的 static（文件作用域）想表达：
 * “这个函数只在当前 cpp 可见（internal linkage）”
 * 但问题是：
 * ❗ 类成员函数不能再被赋予这种“文件级链接属性”

 * 编译报错: error: cannot declare member function 'static void A::func()' to have static linkage [-fpermissive]
 * 原因：static在cpp文件中使用表明该变量或者函数只在该cpp文件作用域内可见（internal linkage），外部不可引用
 * 类函数中的static只应该出现在.h文件中，cpp中不可以再次出现，否则发生语义冲突。
 * 类函数在其他cpp文件中可引用，加上static局限其引用，发生语义冲突
 */
// static void A::func() {

// }

void A::func() {

}
int main() {
    
    return 0;
}