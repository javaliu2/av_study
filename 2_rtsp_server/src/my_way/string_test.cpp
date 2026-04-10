#include <iostream>
#include <string>
#include <cstring>

int main() {
    const char* cc = "hello";
    int len = strlen(cc);
    const char* end = cc + len;
    std::string str(cc, end);  // [, )
    std::cout << str;
    return 0;
}