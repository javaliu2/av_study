#include <iostream>
#include <string>
#include <map>
#include <cstdint>
using namespace std;

class Person {
public:
    Person() {
        cout << "Person()" << endl;
    }
    Person(string name, uint8_t age) : mName(name), mAge(age) {
        cout << "Person(string, uint8_t)" << endl;
    }
    Person(const Person& other) {
        cout << "Person(const Person&). " << "this: " << this << "; other: " << &other << endl;
        mName = other.mName;
        mAge = other.mAge;
    }
    ~Person() {
        cout << "~Person(), address: " << this << endl;
    }
    // 其实就是Java中的toString()
    friend ostream& operator<<(ostream& os, const Person& p) {
        os << "address: " << &p << ". name: " << p.mName << "; age: " << (int)p.mAge << endl;
        return os;
    }
    void setName(string name) {
        mName = name;
    }
private:
    string mName;
    uint8_t mAge;
};

int main() {
    map<int, string> c;
    c[2] = "xs";
    c[1] = "cr";
    c[0] = "aa";

    for (map<int, string>::iterator it = c.begin(); it != c.end(); ++it) {
        cout << "key: " << it->first << "; value: " << it->second << endl;
    }
    /**
     * output:
     * key: 0; value: aa
     * key: 1; value: cr
     * key: 2; value: xs
     */
    // note: map会按照key对pair进行升序排序

    Person p("xs", 26);
    cout << p;  // address: 0x8fdbff800. name: xs; age: 26
    int key = 1;
    map<int, Person> mm;
    mm.insert(std::make_pair(key, p));  // 这里会调用Person的copy-constructor吗
    // 会调用，输出如下三行
    // Person(const Person&). this: 0x8fdbff8c8; other: 0x8fdbff800  // 构造临时对象 0x8fdbff818
    // Person(const Person&). this: 0x235a6181898; other: 0x8fdbff8c8  // 通过临时对象0x8fdbff818构造map中的对象0x235a6181898
    // ~Person(), address: 0x8fdbff8c8 // 析构临时对象 0x8fdbff818
    cout << mm[key];  // address: 0x235a6181898. name: xs; age: 26
    
    cout << "---------mm---------" << endl;
    Person t = mm[key];  // Person(const Person&). this: 0x846efff950; other: 0x235a6181898
    t.setName("cr");
    cout << t;  // address: 0x846efff950. name: cr; age: 26
    mm.erase(key);  // ~Person(), address: 0x235a6181898
    mm.insert(std::make_pair(key, t));
    // 上一行代码输出如下三行
    // Person(const Person&). this: 0x846efffaf8; other: 0x846efff950
    // Person(const Person&). this: 0x235a6181898; other: 0x846efffaf8
    //~Person(), address: 0x846efffaf8
    cout << mm[key];  // address: 0x235a6181898. name: cr; age: 26
    // return 0;
    // ~Person(), address: 0x846efff950  // 析构对象t
    // ~Person(), address: 0x235a6181898  // 析构map中的对象
    // ~Person(), address: 0x8fdbff800  // 最后析构main函数中的对象

    cout << "---------mp---------" << endl;
    map<int, Person*>* mp = new map<int, Person*>();
    Person* p1 = new Person("xs", 26);
    mp->insert(std::make_pair(4, p1));
    cout << *p1;
    delete mp;  // 析构map的时候，并不会调用Person的析构函数，所以需要手动delete对象。现代推荐实践，使用智能指针来管理对象的生命周期，避免内存泄漏和悬空指针的问题。
}