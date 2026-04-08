#include <iostream>
#include <vector>

using namespace std;

class A {
public:
    A() {
        cout << "constructor A()" << endl;
    }
    ~A() {
        cout << "de-constructor A()" << endl;
    }
};

int main() {
    vector<A> arr(4);  // 会调用类A的构造函数
    cout << "arr size: " << arr.size() << endl;
    arr.clear();  // 调用A的析构函数
    cout << "=======" <<  endl;
    vector<int*> ptrs;
    int a = 1, b = 2;
    ptrs.push_back(&a);
    ptrs.push_back(&b);
    for (auto ptr : ptrs) {
        cout << "*ptr: " << *ptr << " ";  // 指针变量的复制，对ptr的修改不影响原值
        ptr = nullptr;
    }
    cout << endl;
    for (auto ptr : ptrs) {
        cout << "*ptr: " << *ptr << " ";
    }
    cout << "\n====" <<endl;
    for (auto& ptr : ptrs) {  // 指针变量的引用，对ptr的修改直接影响到vector中的原始值
        cout << "*ptr: " << *ptr << " ";
        ptr = nullptr;
    }
    cout << endl;
    for (auto ptr : ptrs) {
        // cout << "*ptr: " << (void*)ptr << " ";
        printf("%p ", ptr);
    }
    return 0;
}