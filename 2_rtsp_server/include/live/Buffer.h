#pragma once
#include <stdlib.h>
#include <algorithm>
#include <stdint.h>
#include <assert.h>
/**
 * Buffer结构
 * [0, mReadIndex)是已经读取过数据的区间，可prepend（前向追加）
 * [mReadIndex, mWriteIndex)是从 mBuffer 可读取数据的区间
 * [mWriteIndex, mBufferSize)是向 mBuffer 可写数据的区间
 * 本质是一个滑动窗口，通过两个指针的移动实现数据的读写
 * 
 * |----可prepend----|----可读数据----|----可写空间----|
 * 0            mReadIndex     mWriteIndex     mBufferSize
 */
class Buffer {
public:
    static const int initialSize;
    explicit Buffer();
    ~Buffer();

    /**
     * 可读数据的大小（字节）
     */
    int readableBytes() const {
        return mWriteIndex - mReadIndex;
    }
    /**
     * 可写空间的大小（字节）
     */
    int writableBytes() const {
        return mBufferSize - mWriteIndex;
    }
    /**
     * 可前向追加数据的大小（字节）
     */
    int prependableBytes() const {
        return mReadIndex;
    }
    char* peek() {
        return begin() + mReadIndex;
    }
    const char* peek() const {
        return begin() + mReadIndex;
    }
    const char* findCRLF() const {
        // 检索[mReadIndex, mWriteIndex)可读数据区间内子序列'\r\n'第一次出现的位置
        // 两个序列的指定是通过[char* start, char* end)的方式，所以按照如下方式调用
        // return: 子序列存在，返回子序列的位置；否则返回 last1，即2nd parameter
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }
    const char* findCRLF(const char* start) const {
        // 确保start在可读数据区内
        assert(peek() <= start);
        assert(start <= beginWrite());  // G哥说，等于是可以的，如果start==beginWrite，表示空区间
        const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }
    const char* findLastCrlf() const {
        const char* crlf = std::find_end(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? nullptr : crlf;
    }
    void retrieveReadZero() {
        // 自定义函数（本次读取的数据不全，已读取的数据归零，重新从头读取）
        mReadIndex = 0;
    }
    /**
     * 清空整个缓冲区
     */
    void retrieveAll() {
        mReadIndex = 0;
        mWriteIndex = 0;
    }
    void retrieve(int len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) {
            mReadIndex += len;
        } else {
            retrieveAll();
        }
    }
    /**
     * 将peek()至end的数据标记为已读
     */
    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(beginWrite() >= end);
        retrieve(end - peek());
    }
    char* beginWrite() {
        return begin() + mWriteIndex;
    }
    const char* beginWrite() const {
        return begin() + mWriteIndex;
    }
    void unwrite(int len) {
        assert(len <= readableBytes());
        mWriteIndex -= len;
    }
    void ensureWritableBytes(int len) {
        if (writableBytes() < len) {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }
    void makeSpace(int len) {
        if (writableBytes() + prependableBytes() < len) {
            mBufferSize = mWriteIndex + len;
            mBuffer = (char*)realloc(mBuffer, mBufferSize);
        } else {
            int readable = readableBytes();
            // 将可读数据移动到 mBuffer 起始位置
            std::copy(begin() + mReadIndex, begin() + mWriteIndex, begin());
            mReadIndex = 0;
            mWriteIndex = mReadIndex + readable;
            assert(readable == readableBytes());
        }
    }
    void append(const char* data, int len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        assert(len <= writableBytes());
        mWriteIndex += len;
    }
    void append(const void* data, int len) {
        append((const char*)(data), len);
    }
    // 这里的读写指的是对 fd 的操作
    int read(int fd);
    int write(int fd);
private:
    /**
     * 返回数据缓冲区的起始位置，基(base)
     */
    char* begin() {
        return mBuffer;
    }
    const char* begin() const {
        return mBuffer;
    }
private:
    char* mBuffer;
    int mBufferSize;
    int mReadIndex;
    int mWriteIndex;

    static const char* kCRLF;
};