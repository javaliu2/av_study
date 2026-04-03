#include <stdio.h>

/**
 * hello.txt content:
 * i am xs, and my dharma name is huixiao, meaning that wisdom arises.
 * 一共67个字符，1个字符1字节，所以一共67个字节
 */
int main() {
    FILE* file = nullptr;
    const char* file_path = "../resource/hello.txt";  // exe在build目录，所以是上一级
    file = fopen(file_path, "rb");
    if (!file) {
        printf("open file failed\n");
        return -1;
    }
    fseek(file, 0, SEEK_END);  // 跳到末尾
    long file_size = ftell(file);
    printf("hello.txt size: %ld bytes\n", file_size);  // 67

    // attention! 需要跳到文件开头再读取内容
    fseek(file, 0, SEEK_SET);
    const int len = 8;
    char content[len] = {0};
    int ret = fread(content, 1, len-1, file);
    printf("ret: %d\n", ret);
    if (ret <= 0) {
        printf("fread failed\n");
        return -1;
    }
    content[ret] = '\0';
    printf("content: %s \n", content);

    ret = fread(content, 1, len-1, file);
    printf("ret: %d\n", ret);
    if (ret <= 0) {
        printf("fread failed\n");
        return -1;
    }
    content[ret] = '\0';
    printf("content: %s \n", content);

    fseek(file, len, SEEK_CUR);  // 从当前位置向后跳跃8个字节
    ret = fread(content, 1, len-1, file);
    printf("ret: %d\n", ret);
    if (ret <= 0) {
        printf("fread failed\n");
        return -1;
    }
    content[ret] = '\0';
    printf("content: %s \n", content);
    return 0;
}