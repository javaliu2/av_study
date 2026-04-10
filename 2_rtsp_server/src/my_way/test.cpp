#include <string>
#include <cstdint>

bool parseCSeq(std::string& message) {
    std::size_t pos = message.find("CSeq");
    printf("pos: %d\n", pos);
    if (pos != std::string::npos) {  // npos是固定常量
        uint32_t cseq = 0;
        /**
         * %*[^:]: %u中*表示忽略匹配到的内容，不存到变量
         * 整体意思: 读取并忽略从CSeq开始到冒号前的所有内容
         */
        printf("message + pos: %s\n", message.c_str() + pos);
        sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
        // mCSeq = cseq;
        return true;
    }
    return false;
}

/**
 * output:
 * pos: 9
 * message + pos: CSeq: 3 hello
 */
int main() {
    /**
     * message: DESCRIBE CSeq: 3 hello
     * index:   0123456789ABCDEF
     */
    std::string mes = "DESCRIBE CSeq: 3 hello";
    parseCSeq(mes);
    return 0;
}