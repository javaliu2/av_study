#include "Buffer.h"
#include "scheduler/SocketsOps.h"

const int Buffer::initialSize = 1024;  // 1KB
const char* Buffer::kCRLF = "\r\n";

Buffer::Buffer() : mBufferSize(initialSize), mReadIndex(0), mWriteIndex(0) {
    mBuffer = (char*)malloc(mBufferSize);
}

Buffer::~Buffer() {
    free(mBuffer);
}

int Buffer::read(int fd) {
    char extrabuf[65536];  // 64KB, 源自工程实践，平衡系统调用的次数和拷贝的次数
    // 65536 是一个工程经验值：足够大以减少系统调用，又足够小以保证安全和效率
    const int writable = writableBytes();
    const int n = ::recv(fd, extrabuf, sizeof(extrabuf), 0);
    if (n <= 0) {
        return -1;
    } else if (n <= writable) {
        std::copy(extrabuf, extrabuf + n, beginWrite());
        mWriteIndex += n;
    } else {
        std::copy(extrabuf, extrabuf + writable, beginWrite());
        mWriteIndex += writable;
        append(extrabuf + writable, n - writable);  // 追加剩余的数据
    }
    return n;
}

int Buffer::write(int fd) {
    return sockets::write(fd, peek(), readableBytes());
}