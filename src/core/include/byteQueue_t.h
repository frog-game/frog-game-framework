#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "utility_t.h"

typedef struct byteQueue_s
{
    // private
    char*  pBuffer;       //数据
    size_t nCapacity;     //容量
    size_t nReadIndex;    //读索引
    size_t nWriteIndex;   //写索引
} byteQueue_tt;

frCore_API void byteQueue_init(byteQueue_tt* pByteQueue, size_t nCapacity /* = 256*/);

frCore_API void byteQueue_clear(byteQueue_tt* pByteQueue);

frCore_API void byteQueue_write(byteQueue_tt* pByteQueue, const void* pInBytes, size_t nLength);

frCore_API void byteQueue_writeBytes(byteQueue_tt* pByteQueue, const void* pInBytes,
                                     size_t nLength);

frCore_API void byteQueue_writeChar(byteQueue_tt* pByteQueue, const char c);

frCore_API bool byteQueue_readBytes(byteQueue_tt* pByteQueue, void* pOutBytes,
                                    size_t nMaxLengthToRead, bool bPeek /*= false*/);

frCore_API void byteQueue_reset(byteQueue_tt* pByteQueue);

frCore_API void byteQueue_reserve(byteQueue_tt* pByteQueue, size_t nCapacity);

static inline void byteQueue_swapToReset(byteQueue_tt* pByteQueue, byteQueue_tt* pRhs)
{
    char*  pBuffer          = pByteQueue->pBuffer;
    size_t nCapacity        = pByteQueue->nCapacity;
    pByteQueue->pBuffer     = pRhs->pBuffer;
    pByteQueue->nReadIndex  = pRhs->nReadIndex;
    pByteQueue->nWriteIndex = pRhs->nWriteIndex;
    pByteQueue->nCapacity   = pRhs->nCapacity;
    pRhs->pBuffer           = pBuffer;
    pRhs->nCapacity         = nCapacity;
    pRhs->nReadIndex        = nCapacity;
    pRhs->nWriteIndex       = nCapacity;
}

static inline void byteQueue_swapBuffer(byteQueue_tt* pByteQueue, char** ppBuffer,
                                        size_t* pCapacity)
{
    char*  pBuffer          = pByteQueue->pBuffer;
    size_t nCapacity        = pByteQueue->nCapacity;
    pByteQueue->pBuffer     = *ppBuffer;
    pByteQueue->nReadIndex  = *pCapacity;
    pByteQueue->nWriteIndex = *pCapacity;
    pByteQueue->nCapacity   = *pCapacity;
    *ppBuffer               = pBuffer;
    *pCapacity              = nCapacity;
}

static inline bool byteQueue_empty(byteQueue_tt* pByteQueue)
{
    return (pByteQueue->nReadIndex == pByteQueue->nCapacity);
}

//获取剩余可读字节数
static inline size_t byteQueue_getBytesReadable(byteQueue_tt* pByteQueue)
{
    // pByteQueue->nWriteIndex:已经写入的数据索引[竟然已经写入过了那么同样也说明可以读取数据]
    //(pByteQueue->nCapacity - pByteQueue->nReadIndex)剩余可读取的[容量 - 已经读取过的数据索引]
    // pByteQueue->nWriteIndex + (pByteQueue->nCapacity - pByteQueue->nReadIndex)
    // 已经写入取过的数据索引 + 剩余可读取的
    if (pByteQueue->nWriteIndex > pByteQueue->nReadIndex)   //写索引在读索引右边
    {
        return pByteQueue->nWriteIndex - pByteQueue->nReadIndex;
    }
    else   //写索引在读索引左边
    {
        return pByteQueue->nWriteIndex + (pByteQueue->nCapacity - pByteQueue->nReadIndex);
    }
}

//获取剩余可写字节数
static inline size_t byteQueue_getBytesWritable(byteQueue_tt* pByteQueue)
{
    // pByteQueue->nReadIndex:已经读取过的数据索引[竟然已经读取过了那么同样也说明可以在写入数据]
    //(pByteQueue->nCapacity - pByteQueue->nWriteIndex)剩余可写的[容量 - 已经写过的数据索引]
    // pByteQueue->nReadIndex + (pByteQueue->nCapacity - pByteQueue->nWriteIndex)
    // 已经读取过的数据索引 + )剩余可写的
    if (pByteQueue->nReadIndex >=
        pByteQueue->nWriteIndex)   //写索引在读索引左边或重合（说明写入的数据覆盖了旧的数据）
    {
        return pByteQueue->nReadIndex - pByteQueue->nWriteIndex;
    }
    else   //读索引在写索引右边
    {
        return pByteQueue->nReadIndex + (pByteQueue->nCapacity - pByteQueue->nWriteIndex);
    }
}

static inline size_t byteQueue_getCapacity(byteQueue_tt* pByteQueue)
{
    return pByteQueue->nCapacity;
}

static inline char* byteQueue_getBuffer(byteQueue_tt* pByteQueue)
{
    return pByteQueue->pBuffer;
}

//取出此此容量大小剩余可读数据起始指针和大小
static inline char* byteQueue_peekContiguousBytesRead(byteQueue_tt* pByteQueue, size_t* pReadBytes)
{
    if (pByteQueue->nWriteIndex > pByteQueue->nReadIndex) {
        *pReadBytes = pByteQueue->nWriteIndex - pByteQueue->nReadIndex;
    }
    else {
        *pReadBytes = pByteQueue->nCapacity - pByteQueue->nReadIndex;
    }
    return pByteQueue->pBuffer + pByteQueue->nReadIndex;
}

//取出此容量大小剩余可写数据起始指针和大小
static inline char* byteQueue_peekContiguousBytesWrite(byteQueue_tt* pByteQueue,
                                                       size_t*       pWriteBytes)
{
    if (pByteQueue->nReadIndex >= pByteQueue->nWriteIndex) {
        *pWriteBytes = pByteQueue->nReadIndex - pByteQueue->nWriteIndex;
    }
    else {
        *pWriteBytes = pByteQueue->nCapacity - pByteQueue->nWriteIndex;
    }
    return pByteQueue->pBuffer + pByteQueue->nWriteIndex;
}

static inline void byteQueue_readOffset(byteQueue_tt* pByteQueue, size_t nLength)
{
    assert(byteQueue_getBytesReadable(pByteQueue) >= nLength);

    if (byteQueue_getBytesReadable(pByteQueue) == nLength) {
        pByteQueue->nReadIndex  = pByteQueue->nCapacity;
        pByteQueue->nWriteIndex = 0;
    }
    else {
        pByteQueue->nReadIndex = (pByteQueue->nReadIndex + nLength) % pByteQueue->nCapacity;
    }
}

static inline void byteQueue_writeOffset(byteQueue_tt* pByteQueue, size_t nLength)
{
    assert(nLength != 0);
    assert(byteQueue_getBytesWritable(pByteQueue) >= nLength);
    pByteQueue->nWriteIndex = (pByteQueue->nWriteIndex + nLength) % pByteQueue->nCapacity;
    if (pByteQueue->nReadIndex == pByteQueue->nCapacity) {
        pByteQueue->nReadIndex = 0;
    }
}
