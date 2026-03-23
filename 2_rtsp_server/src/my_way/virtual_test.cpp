#include <iostream>

class Shape {
    virtual void fun() = 0;
};

class Circle : Shape {
public:   
    Circle() {

    }
    void fun() override {
        std::cout << "Circle\n";
    }
};

int main() {
    // Shape s;  // Shape为抽象类，无法实例化
    Circle c;  // 需要实现父类中的所有纯虚函数，否则是抽象类

    return 0;
}