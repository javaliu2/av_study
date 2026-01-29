#include <iostream>
#include <stdio.h>
using namespace std;
int g_cnt = 1;

void func(int* p) {
    cout << "in func\n";
    cout << "value of p: " << p << endl;
    p = &g_cnt;
    cout << "after assign, value of p: " << p << endl;
    cout << "end func\n";
}

void func2(int** p) {
    cout << "in func2\n";
    cout << "value of *p: " << *p << endl;
    *p = &g_cnt;
    cout << "after assign, value of *p: " << *p << endl;
    cout << "end func2\n";
}

void func3(const void** p) {
    // *p的是一个指向void类型的指针，const关键字表示不能通过*p修改其指向的内容
    // 但是可以修改*p本身，即改变它指向的内容
    *p = "hello world";
    printf("*p: %p, *p->content: %s\n", *p, *p);
    *p = "hello xs";
    printf("*p: %p, *p->content: %s\n", *p, *p);
    // p是一个二级指针，他的值可以被修改，即改变其指向的内容
    char* t = "abc";
    p = (const void**)&t;  // 修改的是形参p，如果要在func3中修改p对应实参的值，需要传递三级指针
    printf("*p: %p, *p->content: %s\n", *p, *p);
    // 不能修改*p指向的内容
    // ((char*)*p)[1] = 'x';
}

/**
 * G-bro:
 * 1、func 中只有一个二级指针变量 p（值传递），p指向的对象并不属于 func，而是 main 中的指针变量。
 *    因此所有对 *p 的读写，都是在操作 main 的那个指针变量本身.
 * 2、二级指针的意义只有一个：允许函数修改“调用者的指针变量本身”
 */
void func4(void** p) {
    char t[] = "func4";  // &t: 0x5ffdda
    t[0] = 'a';
    // p = (void**)&t; // p: 0x5ffdda，不存在二级指针解析，即t的值不能被正确赋值给*p
    // *p存在于调用者内存区域内，func4中只有一个二级指针变量的内存空间，没有一级指针变量的内存空间
    *p = t;  // 正确写法。呼应G-bro总结的第2点
    // p = (void**)(&(&t)); // &t是一个地址，为右值，右值不能再被取地址
    // 上面这样赋值以后，*p为(void*)类型
    printf("*p: %p, *p->content: %s\n", *p, *p);
    ((char*)*p)[1] = 'x';
    printf("*p: %p, *p->content: %s\n", *p, *p);
}

int main() {
    int a = 1;
    int* main_p = &a;
    cout << "address of g_cnt: " << &g_cnt << endl;
    cout << "in main\n";
    cout << "value of main_p: " << main_p << endl;
    func(main_p);
    cout << "after calling func, value of main_p: " << main_p << endl;
    int** main_pp = &main_p;
    cout << "======" << endl;
    cout << "value of *main_pp: " << *main_pp << endl;
    func2(main_pp);
    cout << "after calling func2, value of *main_pp: " << *main_pp << endl;
    cout << "after calling func2, value of main_p: " << main_p << endl;
    cout << "====== func3 test ======" << endl;
    const void* ptr = nullptr;
    const void** char_pp = &ptr;
    func3(char_pp);
    printf("after func3, ptr: %p, ptr->content: %s\n", ptr, ptr);
    printf("after func3, char_pp: %p, char_pp->content: %p\n", char_pp, *char_pp);  // char_pp本身的地址，*char_pp指向的对象，也是指针，它的地址
    void* ptr2 = nullptr;
    void** char_pp2 = &ptr2;
    func4(char_pp2);
}
