#include <iostream>
#include <string>
#include <map>

int main() {
    using namespace std;
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
    return 0;
}