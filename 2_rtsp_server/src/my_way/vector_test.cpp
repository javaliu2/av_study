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
    arr.clear();
    return 0;
}