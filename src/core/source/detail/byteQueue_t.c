#include "byteQueue_t.h"

#include <stdlib.h>
#include <string.h>

void byteQueue_init(byteQueue_tt* pByteQueue, size_t nCapacity)
{
    pByteQueue->nReadIndex  = nCapacity;
    pByteQueue->nWriteIndex = 0;
    pByteQueue->nCapacity   = nCapacity;
    if (nCapacity != 0) {
        pByteQueue->pBuffer = mem_malloc(nCapacity);
    }
    else {
        pByteQueue->pBuffer = NULL;
    }
}

void byteQueue_clear(byteQueue_tt* pByteQueue)
{
    pByteQueue->nReadIndex  = 0;
    pByteQueue->nWriteIndex = 0;
    pByteQueue->nCapacity   = 0;
    if (pByteQueue->pBuffer) {
        mem_free(pByteQueue->pBuffer);
        pByteQueue->pBuffer = NULL;
    }
}

void byteQueue_writeChar(byteQueue_tt* pByteQueue, const char c)
{
    size_t nBytesWritable = byteQueue_getBytesWritable(pByteQueue);   //获取可写字节数
    if (pByteQueue->nCapacity == 0) {
        //初始化容量,buffer大小，可读索引
        pByteQueue->nCapacity  = 256;
        pByteQueue->pBuffer    = mem_malloc(pByteQueue->nCapacity);
        pByteQueue->nReadIndex = pByteQueue->nCapacity;
    }
    else {
        if (pByteQueue->nCapacity == 0 || 1 > nBytesWritable) {
            // align_size  将size按align大小整数倍提升
            size_t nNewCapacity = align_size((pByteQueue->nCapacity << 1) + 1, 256);

            char* pBuffer = mem_malloc(nNewCapacity);
            if (pByteQueue->nReadIndex !=
                pByteQueue->nCapacity)   //说明还有可读数据需要放入新生成空间
            {
                size_t nWritten   = byteQueue_getBytesReadable(pByteQueue);
                size_t nReadBytes = 0;
                char*  pRead      = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
                memcpy(pBuffer, pRead, nReadBytes);
                if (nReadBytes != nWritten)   //如果已读的!=可读的字节大小
                {
                    //把剩下没有读取的数据拷贝到新的buff容器
                    memcpy(pBuffer + nReadBytes, pByteQueue->pBuffer, nWritten - nReadBytes);
                }
                pByteQueue->nReadIndex  = 0;
                pByteQueue->nWriteIndex = nWritten;
            }
            else {
                pByteQueue->nReadIndex  = nNewCapacity;
                pByteQueue->nWriteIndex = 0;
            }
            pByteQueue->nCapacity = nNewCapacity;
            mem_free(pByteQueue->pBuffer);
            pByteQueue->pBuffer = pBuffer;
        }
    }

    pByteQueue->pBuffer[pByteQueue->nWriteIndex] = c;   //赋值
    pByteQueue->nWriteIndex =
        (pByteQueue->nWriteIndex + 1) % pByteQueue->nCapacity;   //索引位移一位
    if (pByteQueue->nReadIndex == pByteQueue->nCapacity)         //如果读索引在尾部
    {
        pByteQueue->nReadIndex = 0;   //把读索引放到头部
    }
}

void byteQueue_write(byteQueue_tt* pByteQueue, const void* pInBytes, size_t nLength)
{
    assert(nLength != 0);
    if (pByteQueue->nCapacity == 0) {
        pByteQueue->nCapacity  = align_size(nLength, 256);
        pByteQueue->pBuffer    = mem_malloc(pByteQueue->nCapacity);
        pByteQueue->nReadIndex = pByteQueue->nCapacity;
    }
    else {
        size_t nBytesWritable = byteQueue_getBytesWritable(pByteQueue);
        if (nLength > nBytesWritable) {
            size_t nNewCapacity =
                align_size((pByteQueue->nCapacity << 1) + (nLength - nBytesWritable), 256);

            char* pBuffer = mem_malloc(nNewCapacity);
            if (pByteQueue->nReadIndex != pByteQueue->nCapacity) {
                size_t nWritten   = byteQueue_getBytesReadable(pByteQueue);
                size_t nReadBytes = 0;
                char*  pRead      = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
                memcpy(pBuffer, pRead, nReadBytes);
                if (nReadBytes != nWritten) {
                    memcpy(pBuffer + nReadBytes, pByteQueue->pBuffer, nWritten - nReadBytes);
                }
                pByteQueue->nReadIndex  = 0;
                pByteQueue->nWriteIndex = nWritten;
            }
            else {
                pByteQueue->nReadIndex  = nNewCapacity;
                pByteQueue->nWriteIndex = 0;
            }
            pByteQueue->nCapacity = nNewCapacity;
            mem_free(pByteQueue->pBuffer);
            pByteQueue->pBuffer = pBuffer;
        }
    }

    size_t nWriteBytes = 0;
    char*  pWrite      = byteQueue_peekContiguousBytesWrite(pByteQueue, &nWriteBytes);

    if (nWriteBytes >= nLength) {
        memcpy(pWrite, pInBytes, nLength);
    }
    else {
        memcpy(pWrite, pInBytes, nWriteBytes);
        memcpy(pByteQueue->pBuffer, (const char*)pInBytes + nWriteBytes, nLength - nWriteBytes);
    }
    pByteQueue->nWriteIndex = (pByteQueue->nWriteIndex + nLength) % pByteQueue->nCapacity;
    if (pByteQueue->nReadIndex == pByteQueue->nCapacity) {
        pByteQueue->nReadIndex = 0;
    }
}

void byteQueue_writeBytes(byteQueue_tt* pByteQueue, const void* pInBytes, size_t nLength)
{
    assert(nLength != 0);
    if (pByteQueue->nCapacity == 0) {
        pByteQueue->nCapacity  = nLength;
        pByteQueue->pBuffer    = mem_malloc(pByteQueue->nCapacity);
        pByteQueue->nReadIndex = pByteQueue->nCapacity;
    }
    else {
        size_t nBytesWritable = byteQueue_getBytesWritable(pByteQueue);
        if (nLength > nBytesWritable) {
            size_t nNewCapacity = pByteQueue->nCapacity + (nLength - nBytesWritable);
            char*  pBuffer      = mem_malloc(nNewCapacity);
            if (pByteQueue->nReadIndex != pByteQueue->nCapacity) {
                size_t nWritten   = byteQueue_getBytesReadable(pByteQueue);
                size_t nReadBytes = 0;
                char*  pRead      = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
                memcpy(pBuffer, pRead, nReadBytes);
                if (nReadBytes != nWritten) {
                    memcpy(pBuffer + nReadBytes, pByteQueue->pBuffer, nWritten - nReadBytes);
                }
                pByteQueue->nReadIndex  = 0;
                pByteQueue->nWriteIndex = nWritten;
            }
            else {
                pByteQueue->nReadIndex  = nNewCapacity;
                pByteQueue->nWriteIndex = 0;
            }
            pByteQueue->nCapacity = nNewCapacity;
            mem_free(pByteQueue->pBuffer);
            pByteQueue->pBuffer = pBuffer;
        }
    }

    size_t nWriteBytes = 0;
    char*  pWrite      = byteQueue_peekContiguousBytesWrite(pByteQueue, &nWriteBytes);
    if (nWriteBytes >= nLength) {
        memcpy(pWrite, pInBytes, nLength);
    }
    else {
        memcpy(pWrite, pInBytes, nWriteBytes);
        memcpy(pByteQueue->pBuffer, (const char*)pInBytes + nWriteBytes, nLength - nWriteBytes);
    }
    pByteQueue->nWriteIndex = (pByteQueue->nWriteIndex + nLength) % pByteQueue->nCapacity;
    if (pByteQueue->nReadIndex == pByteQueue->nCapacity) {
        pByteQueue->nReadIndex = 0;
    }
}

bool byteQueue_readBytes(byteQueue_tt* pByteQueue, void* pOutBytes, size_t nMaxLengthToRead,
                         bool bPeek /*= false*/)
{
    size_t nBytesWritten = byteQueue_getBytesReadable(pByteQueue);
    size_t nBytesToRead  = nBytesWritten < nMaxLengthToRead ? nBytesWritten : nMaxLengthToRead;
    if (nBytesToRead == 0) {
        return false;
    }

    size_t nReadBytes = 0;
    char*  pRead      = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
    if (nReadBytes >= nBytesToRead) {
        memcpy(pOutBytes, pRead, nBytesToRead);
    }
    else {
        memcpy(pOutBytes, pRead, nReadBytes);
        memcpy((char*)pOutBytes + nReadBytes, pByteQueue->pBuffer, nBytesToRead - nReadBytes);
    }

    if (!bPeek) byteQueue_readOffset(pByteQueue, nBytesToRead);

    return true;
}

void byteQueue_reset(byteQueue_tt* pByteQueue)
{
    pByteQueue->nReadIndex  = pByteQueue->nCapacity;
    pByteQueue->nWriteIndex = 0;
}

void byteQueue_reserve(byteQueue_tt* pByteQueue, size_t nCapacity)
{
    size_t nWritten = byteQueue_getBytesReadable(pByteQueue);
    if (nWritten > nCapacity) {
        return;
    }

    if (pByteQueue->nReadIndex != pByteQueue->nCapacity) {
        char*  pBuffer    = mem_malloc(nCapacity);
        size_t nReadBytes = 0;
        char*  pRead      = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
        memcpy(pBuffer, pRead, nReadBytes);
        if (nReadBytes != nWritten) {
            memcpy(pBuffer + nReadBytes, pByteQueue->pBuffer, nWritten - nReadBytes);
        }
        pByteQueue->nReadIndex  = 0;
        pByteQueue->nWriteIndex = nWritten;
        mem_free(pByteQueue->pBuffer);
        pByteQueue->pBuffer = pBuffer;
    }
    else {
        pByteQueue->pBuffer     = mem_realloc(pByteQueue->pBuffer, nCapacity);
        pByteQueue->nReadIndex  = nCapacity;
        pByteQueue->nWriteIndex = 0;
    }
    pByteQueue->nCapacity = nCapacity;
}