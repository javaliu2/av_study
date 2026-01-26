#include <iostream>

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
    return 0;
}
