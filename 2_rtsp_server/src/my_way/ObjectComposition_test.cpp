#include <iostream>
using namespace std;

class A {
public:
    A() {
        cout << "A()" << endl;
    }
};

class B {
    char* ptr;
    int num;
    A a;
public:
    B() {
        cout << "B()" << endl;
        cout << "ptr:" << &ptr << endl;
        cout << "num:" << num << endl;
    }
};
/**
 * output:
 * A()
 * B()
 * ptr:0xb951ffb60
 * num:-1213763520
 * 会自动调用A的构造函数
 */
int main() {
    B b;
    return 0;
}