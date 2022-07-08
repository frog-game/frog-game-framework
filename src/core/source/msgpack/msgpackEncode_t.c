#include "msgpack/msgpackEncode_t.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "platform_t.h"
#include "utility_t.h"

static inline size_t Max(const size_t a, const size_t b)
{
    return a >= b ? a : b;
}

#define IS_INT_TYPE_EQUIVALENT(x, T) (!isinf(x) && (T)(x) == (x))

#define IS_INT64_EQUIVALENT(x) IS_INT_TYPE_EQUIVALENT(x, int64_t)

static inline uint8_t* msgpackEncode_getBuffer(msgpackEncode_tt* pMsgpackEncode)
{
    return pMsgpackEncode->pBuffer + pMsgpackEncode->nOffset;
}

static inline void msgpackEncode_append(msgpackEncode_tt* pMsgpackEncode, size_t nLength)
{
    if (nLength + pMsgpackEncode->nOffset > pMsgpackEncode->nLength) {
        while (nLength + pMsgpackEncode->nOffset > pMsgpackEncode->nLength) {
            pMsgpackEncode->nLength *= 2;
        }
        if (pMsgpackEncode->fnExpand) {
            pMsgpackEncode->pBuffer = pMsgpackEncode->fnExpand(pMsgpackEncode->pBuffer,
                                                               pMsgpackEncode->nOffset,
                                                               pMsgpackEncode->nLength,
                                                               pMsgpackEncode->pExpandData);
        }
        else {
            pMsgpackEncode->pBuffer = mem_realloc(pMsgpackEncode->pBuffer, pMsgpackEncode->nLength);
        }
    }
}

void msgpackEncode_init(msgpackEncode_tt* pMsgpackEncode, void* pBuffer, size_t nLength,
                        void* (*fnExpand)(void*, size_t, size_t, void*), void* pExpandData)
{
    pMsgpackEncode->nOffset     = 0;
    pMsgpackEncode->fnExpand    = fnExpand;
    pMsgpackEncode->pExpandData = pExpandData;
    if (pBuffer == NULL) {
        pMsgpackEncode->nLength = Max(nLength, 256);
        if (pMsgpackEncode->fnExpand) {
            pMsgpackEncode->pBuffer = pMsgpackEncode->fnExpand(
                NULL, 0, pMsgpackEncode->nLength, pMsgpackEncode->pExpandData);
        }
        else {
            pMsgpackEncode->pBuffer = mem_malloc(pMsgpackEncode->nLength);
        }
    }
    else {
        pMsgpackEncode->nLength = nLength;
        pMsgpackEncode->pBuffer = pBuffer;
    }
}

void msgpackEncode_clear(msgpackEncode_tt* pMsgpackEncode)
{
    if (pMsgpackEncode) {
        if (pMsgpackEncode->pBuffer) {
            if (pMsgpackEncode->fnExpand) {
                pMsgpackEncode->fnExpand(
                    pMsgpackEncode->pBuffer, 0, 0, pMsgpackEncode->pExpandData);
                pMsgpackEncode->pBuffer = NULL;
            }
            else {
                mem_free(pMsgpackEncode->pBuffer);
                pMsgpackEncode->pBuffer = NULL;
            }
        }
    }
}

void msgpackEncode_writeInteger(msgpackEncode_tt* pMsgpackEncode, int64_t iValue)
{
    if (iValue >= 0) {
        if (iValue <= 127) {
            msgpackEncode_append(pMsgpackEncode, 1);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = (uint8_t)(iValue & 0x7f);
            pMsgpackEncode->nOffset += 1;
        }
        else if (iValue <= 0xff) {
            msgpackEncode_append(pMsgpackEncode, 2);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xcc;
            pBuffer[1]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 2;
        }
        else if (iValue <= 0xffff) {
            msgpackEncode_append(pMsgpackEncode, 3);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xcd;
            pBuffer[1]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[2]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 3;
        }
        else if (iValue <= 0xffffffffLL) {
            msgpackEncode_append(pMsgpackEncode, 5);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xce;
            pBuffer[1]       = (uint8_t)((iValue & 0xff000000) >> 24);
            pBuffer[2]       = (uint8_t)((iValue & 0xff0000) >> 16);
            pBuffer[3]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[4]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 5;
        }
        else {
            msgpackEncode_append(pMsgpackEncode, 9);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xcf;
            pBuffer[1]       = (uint8_t)((iValue & 0xff00000000000000LL) >> 56);
            pBuffer[2]       = (uint8_t)((iValue & 0xff000000000000LL) >> 48);
            pBuffer[3]       = (uint8_t)((iValue & 0xff0000000000LL) >> 40);
            pBuffer[4]       = (uint8_t)((iValue & 0xff00000000LL) >> 32);
            pBuffer[5]       = (uint8_t)((iValue & 0xff000000) >> 24);
            pBuffer[6]       = (uint8_t)((iValue & 0xff0000) >> 16);
            pBuffer[7]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[8]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 9;
        }
    }
    else {
        if (iValue >= -32) {
            msgpackEncode_append(pMsgpackEncode, 1);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = (uint8_t)(iValue);
            pMsgpackEncode->nOffset += 1;
        }
        else if (iValue >= -128) {
            msgpackEncode_append(pMsgpackEncode, 2);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xd0;
            pBuffer[1]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 2;
        }
        else if (iValue >= -32768) {
            msgpackEncode_append(pMsgpackEncode, 3);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xd1;
            pBuffer[1]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[2]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 3;
        }
        else if (iValue >= -2147483648LL) {
            msgpackEncode_append(pMsgpackEncode, 5);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xd2;
            pBuffer[1]       = (uint8_t)((iValue & 0xff000000) >> 24);
            pBuffer[2]       = (uint8_t)((iValue & 0xff0000) >> 16);
            pBuffer[3]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[4]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 5;
        }
        else {
            msgpackEncode_append(pMsgpackEncode, 9);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xd3;
            pBuffer[1]       = (uint8_t)((iValue & 0xff00000000000000LL) >> 56);
            pBuffer[2]       = (uint8_t)((iValue & 0xff000000000000LL) >> 48);
            pBuffer[3]       = (uint8_t)((iValue & 0xff0000000000LL) >> 40);
            pBuffer[4]       = (uint8_t)((iValue & 0xff00000000LL) >> 32);
            pBuffer[5]       = (uint8_t)((iValue & 0xff000000) >> 24);
            pBuffer[6]       = (uint8_t)((iValue & 0xff0000) >> 16);
            pBuffer[7]       = (uint8_t)((iValue & 0xff00) >> 8);
            pBuffer[8]       = (uint8_t)(iValue & 0xff);
            pMsgpackEncode->nOffset += 9;
        }
    }
}

void msgpackEncode_writeBoolean(msgpackEncode_tt* pMsgpackEncode, bool bValue)
{
    msgpackEncode_append(pMsgpackEncode, 1);
    uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
    pBuffer[0]       = bValue ? 0xc3 : 0xc2;
    pMsgpackEncode->nOffset += 1;
}

void msgpackEncode_writeReal(msgpackEncode_tt* pMsgpackEncode, double fValue)
{
    if (IS_INT64_EQUIVALENT(fValue)) {
        msgpackEncode_writeInteger(pMsgpackEncode, (int64_t)fValue);
    }
    else {
        float f = (float)fValue;
        if (fValue == (double)f) {
            msgpackEncode_append(pMsgpackEncode, 5);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xca; /* float IEEE 754 */
            uint8_t* pData   = (uint8_t*)&f;
            pBuffer[1]       = pData[0];
            pBuffer[2]       = pData[1];
            pBuffer[3]       = pData[2];
            pBuffer[4]       = pData[3];
            pMsgpackEncode->nOffset += 5;
        }
        else {
            msgpackEncode_append(pMsgpackEncode, 9);
            uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
            pBuffer[0]       = 0xcb; /* double IEEE 754  */
            uint8_t* pData   = (uint8_t*)&fValue;
            pBuffer[1]       = pData[0];
            pBuffer[2]       = pData[1];
            pBuffer[3]       = pData[2];
            pBuffer[4]       = pData[3];
            pBuffer[5]       = pData[4];
            pBuffer[6]       = pData[5];
            pBuffer[7]       = pData[6];
            pBuffer[8]       = pData[7];
            pMsgpackEncode->nOffset += 9;
        }
    }
}

void msgpackEncode_writeString(msgpackEncode_tt* pMsgpackEncode, const char* szValue,
                               size_t nLength)
{
    if (nLength < 32) {
        msgpackEncode_append(pMsgpackEncode, 1);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xa0 | (nLength & 0xff);   // fixstr
        pMsgpackEncode->nOffset += 1;
    }
    else if (nLength <= 0xff) {
        msgpackEncode_append(pMsgpackEncode, 2);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xd9;
        pBuffer[1]       = (uint8_t)nLength;
        pMsgpackEncode->nOffset += 2;
    }
    else if (nLength <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xda;
        pBuffer[1]       = (uint8_t)((nLength & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(nLength & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xdb;
        pBuffer[1]       = (uint8_t)((nLength & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((nLength & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((nLength & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(nLength & 0xff);
        pMsgpackEncode->nOffset += 5;
    }
    if (nLength > 0) {
        msgpackEncode_append(pMsgpackEncode, nLength);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        memcpy(pBuffer, szValue, nLength);
        pMsgpackEncode->nOffset += nLength;
    }
}

void msgpackEncode_writeBinary(msgpackEncode_tt* pMsgpackEncode, const char* pValue, size_t nLength)
{
    if (nLength <= 0xff) {
        msgpackEncode_append(pMsgpackEncode, 2);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc4;
        pBuffer[1]       = (uint8_t)nLength;
        pMsgpackEncode->nOffset += 2;
    }
    else if (nLength <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc5;
        pBuffer[1]       = (uint8_t)((nLength & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(nLength & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc6;
        pBuffer[1]       = (uint8_t)((nLength & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((nLength & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((nLength & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(nLength & 0xff);
        pMsgpackEncode->nOffset += 5;
    }

    if (nLength > 0) {
        msgpackEncode_append(pMsgpackEncode, nLength);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        memcpy(pBuffer, pValue, nLength);
        pMsgpackEncode->nOffset += nLength;
    }
}

void msgpackEncode_writeArray(msgpackEncode_tt* pMsgpackEncode, int64_t iValue)
{
    if (iValue <= 15) {
        msgpackEncode_append(pMsgpackEncode, 1);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0x90 | (iValue & 0xf);
        pMsgpackEncode->nOffset += 1;
    }
    else if (iValue <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xdc;
        pBuffer[1]       = (uint8_t)((iValue & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(iValue & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xdd;
        pBuffer[1]       = (uint8_t)((iValue & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((iValue & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((iValue & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(iValue & 0xff);
        pMsgpackEncode->nOffset += 5;
    }
}

void msgpackEncode_writeMap(msgpackEncode_tt* pMsgpackEncode, int64_t iValue)
{
    if (iValue <= 15) {
        msgpackEncode_append(pMsgpackEncode, 1);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0x80 | (iValue & 0xf); /* fix map */
        pMsgpackEncode->nOffset += 1;
    }
    else if (iValue <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xde; /* map 16 */
        pBuffer[1]       = (uint8_t)((iValue & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(iValue & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xdf; /* map 32 */
        pBuffer[1]       = (uint8_t)((iValue & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((iValue & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((iValue & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(iValue & 0xff);
        pMsgpackEncode->nOffset += 5;
    }
}

void msgpackEncode_writeUserPointer(msgpackEncode_tt* pMsgpackEncode, void* pValue)
{
    size_t nValue = (size_t)(pValue);
#ifdef DEF_PLATFORM_64BITS
    if (nValue <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc7;
        pBuffer[1]       = (uint8_t)((nValue & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else if (nValue <= 0xffffffffLL) {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc8;
        pBuffer[1]       = (uint8_t)((nValue & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((nValue & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((nValue & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 5;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 9);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc9;
        pBuffer[1]       = (uint8_t)((nValue & 0xff00000000000000LL) >> 56);
        pBuffer[2]       = (uint8_t)((nValue & 0xff000000000000LL) >> 48);
        pBuffer[3]       = (uint8_t)((nValue & 0xff0000000000LL) >> 40);
        pBuffer[4]       = (uint8_t)((nValue & 0xff00000000LL) >> 32);
        pBuffer[5]       = (uint8_t)((nValue & 0xff000000) >> 24);
        pBuffer[6]       = (uint8_t)((nValue & 0xff0000) >> 16);
        pBuffer[7]       = (uint8_t)((nValue & 0xff00) >> 8);
        pBuffer[8]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 9;
    }
#else
    if (nValue <= 0xff) {
        msgpackEncode_append(pMsgpackEncode, 2);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc7;
        pBuffer[1]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 2;
    }
    else if (nValue <= 0xffff) {
        msgpackEncode_append(pMsgpackEncode, 3);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc8;
        pBuffer[1]       = (uint8_t)((nValue & 0xff00) >> 8);
        pBuffer[2]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 3;
    }
    else {
        msgpackEncode_append(pMsgpackEncode, 5);
        uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
        pBuffer[0]       = 0xc9;
        pBuffer[1]       = (uint8_t)((nValue & 0xff000000) >> 24);
        pBuffer[2]       = (uint8_t)((nValue & 0xff0000) >> 16);
        pBuffer[3]       = (uint8_t)((nValue & 0xff00) >> 8);
        pBuffer[4]       = (uint8_t)(nValue & 0xff);
        pMsgpackEncode->nOffset += 5;
    }
#endif
}

void msgpackEncode_writeNil(msgpackEncode_tt* pMsgpackEncode)
{
    msgpackEncode_append(pMsgpackEncode, 1);
    uint8_t* pBuffer = msgpackEncode_getBuffer(pMsgpackEncode);
    pBuffer[0]       = 0xc0;
    pMsgpackEncode->nOffset += 1;
}

void msgpackEncode_swap(msgpackEncode_tt* pMsgpackEncode, void** ppBuffer, size_t* pLength,
                        size_t* pOffset)
{
    uint8_t* pTempBuffer    = pMsgpackEncode->pBuffer;
    pMsgpackEncode->pBuffer = *(uint8_t**)ppBuffer;
    *(uint8_t**)ppBuffer    = pTempBuffer;

    size_t nTempLength      = pMsgpackEncode->nLength;
    pMsgpackEncode->nLength = *pLength;
    *pLength                = nTempLength;
    if (pOffset != NULL) {
        size_t nTempOffset      = pMsgpackEncode->nOffset;
        pMsgpackEncode->nOffset = *pOffset;
        *pOffset                = nTempOffset;
    }
}
