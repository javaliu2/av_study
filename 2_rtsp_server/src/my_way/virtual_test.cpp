#include <iostream>
#include "a.h"
#include "b.h"
#include "c.h"
using namespace std;
/**
 * 爷爷类中的virtual函数，在父类中加override关键字表明对其重写
 * 父类中不必再加virtual关键字，被virtual修饰的函数特性（多态、可继承性）不会丢失
 * 孙子类还可继承父类，重写父类的实现
 */
void A::fun() {
    std::cout << "A:fun()\n";
}
/**
 * cpp中不可加override，加了的话，报错：error: virt-specifiers in 'fun' not allowed outside a class definition
 */
void B::fun() {
    std::cout << "B:fun()\n";
}

// void C::fun() {
//     std::cout << "C:fun()\n";
// }

class Shape {
public:
    Shape() {
        cout << "Shape()" << endl;
    }
    virtual void fun() = 0;
    virtual ~Shape() {
        cout << "~Shape()" << endl;
    }
};
// public表示is-a关系, 继承父类所有的成员，但只能访问public和protected的，private访问不到
class Circle : public Shape {
public:   
    Circle() {
        cout << "Circle()" << endl;
    }
    ~Circle() {
        cout << "~Circle()" << endl;
    }
    void fun() override {
        std::cout << "Circle\n";
    }
};

int main() {
    // Shape s;  // Shape为抽象类，无法实例化
    // Circle c;  // 需要实现父类中的所有纯虚函数，否则是抽象类
    // A a;  // 就算有纯虚函数的实现，这里还是无法实例化抽象类
    A* b = new B();
    b->fun();
    b = new C();
    b->fun();

    /**
     * ~Shape()不加virtual，输出:
     * Shape()
     * Circle()
     * ~Shape()
     * 
     * 加了virtual，输出:
     * Shape()
     * Circle()
     * ~Circle()
     * ~Shape()
     */
    Shape* sc = new Circle();
    delete sc;
    return 0;
}