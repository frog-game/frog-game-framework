#include "msgpack/msgpackDecode_t.h"
#include "platform_t.h"
#include <stdlib.h>
#include <string.h>

static inline const uint8_t* msgpackDecode_getBuffer(msgpackDecode_tt* pMsgpackDecode)
{
    return pMsgpackDecode->pBuffer + pMsgpackDecode->nOffset;
}

void msgpackDecode_init(msgpackDecode_tt* pMsgpackDecode, const uint8_t* pBuffer, size_t nLength,
                        size_t nOffset /*= 0*/)
{
    pMsgpackDecode->pBuffer = pBuffer;
    pMsgpackDecode->nOffset = nOffset;
    pMsgpackDecode->nLength = nLength;
}

void msgpackDecode_clear(msgpackDecode_tt* pMsgpackDecode)
{
    pMsgpackDecode->pBuffer = NULL;
    pMsgpackDecode->nOffset = 0;
    pMsgpackDecode->nLength = 0;
}

bool msgpackDecode_peekInteger(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = pBuffer[1];
    } break;
    case 0xd0: /* int 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int8_t)pBuffer[1];
    } break;
    case 0xcd: /* uint 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2];
    } break;
    case 0xd1: /* int 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int16_t)((uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2]);
    } break;
    case 0xce: /* uint 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (uint32_t)pBuffer[1] << 24 | (uint32_t)pBuffer[2] << 16 |
                  (uint32_t)pBuffer[3] << 8 | (uint32_t)pBuffer[4];
    } break;
    case 0xd2: /* int 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int32_t)((uint32_t)pBuffer[1] << 24 | (uint32_t)pBuffer[2] << 16 |
                            (uint32_t)pBuffer[3] << 8 | (uint32_t)pBuffer[4]);
    } break;
    case 0xcf: /* uint 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int64_t)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                            (uint64_t)pBuffer[3] << 40 | ((uint64_t)(pBuffer[4]) << 32) |
                            (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                            (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]);
    } break;
    case 0xd3: /* int 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int64_t)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                            (uint64_t)pBuffer[3] << 40 | ((uint64_t)(pBuffer[4]) << 32) |
                            (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                            (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]);
    } break;
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            *pValue = pBuffer[0];
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            *pValue = (int8_t)pBuffer[0];
        }
        else {
            return false;
        }
    }
    }
    return true;
}

bool msgpackDecode_readInteger(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = pBuffer[1];
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xd0: /* int 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int8_t)pBuffer[1];
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xcd: /* uint 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2];
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xd1: /* int 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int16_t)((uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xce: /* uint 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (uint32_t)pBuffer[1] << 24 | (uint32_t)pBuffer[2] << 16 |
                  (uint32_t)pBuffer[3] << 8 | (uint32_t)pBuffer[4];
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xd2: /* int 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int32_t)((int32_t)pBuffer[1] << 24 | (int32_t)pBuffer[2] << 16 |
                            (int32_t)pBuffer[3] << 8 | (int32_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xcf: /* uint 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int64_t)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                            (uint64_t)pBuffer[3] << 40 | ((uint64_t)(pBuffer[4]) << 32) |
                            (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                            (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]);
        pMsgpackDecode->nOffset += 9;
    } break;
    case 0xd3: /* int 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (int64_t)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                            (uint64_t)pBuffer[3] << 40 | ((uint64_t)(pBuffer[4]) << 32) |
                            (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                            (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]);
        pMsgpackDecode->nOffset += 9;
    } break;
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            *pValue = pBuffer[0];
            pMsgpackDecode->nOffset += 1;
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            *pValue = (int8_t)pBuffer[0];
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    }
    }
    return true;
}

bool msgpackDecode_readBoolean(msgpackDecode_tt* pMsgpackDecode, bool* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xc3:
    {
        *pValue = true;
        ++pMsgpackDecode->nOffset;
    } break;
    case 0xc2:
    {
        *pValue = false;
        ++pMsgpackDecode->nOffset;
    } break;
    default:
    {
        return false;
    }
    }
    return true;
}

bool msgpackDecode_readReal(msgpackDecode_tt* pMsgpackDecode, double* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)(pBuffer[1]);
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xd0: /* int 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((int8_t)pBuffer[1]);
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xcd: /* uint 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xd1: /* int 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((int16_t)((uint16_t)pBuffer[1] << 8 | (uint16_t)pBuffer[2]));
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xce: /* uint 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((uint32_t)pBuffer[1] << 24 | (uint32_t)pBuffer[2] << 16 |
                           (uint32_t)pBuffer[3] << 8 | (uint32_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xd2: /* int 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((int32_t)((uint32_t)pBuffer[1] << 24 | (uint32_t)pBuffer[2] << 16 |
                                     (uint32_t)pBuffer[3] << 8 | (uint32_t)pBuffer[4]));
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xcf: /* uint 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                           (uint64_t)pBuffer[3] << 40 | (uint64_t)pBuffer[4] << 32 |
                           (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                           (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]);
        pMsgpackDecode->nOffset += 9;
    } break;
    case 0xd3: /* int 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *pValue = (double)((int64_t)((uint64_t)pBuffer[1] << 56 | (uint64_t)pBuffer[2] << 48 |
                                     (uint64_t)pBuffer[3] << 40 | (uint64_t)pBuffer[4] << 32 |
                                     (uint64_t)pBuffer[5] << 24 | (uint64_t)pBuffer[6] << 16 |
                                     (uint64_t)pBuffer[7] << 8 | (uint64_t)pBuffer[8]));
        pMsgpackDecode->nOffset += 9;
    } break;
    case 0xca: /* float */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }

        float    fData = 0.0f;
        uint8_t* pData = (uint8_t*)&fData;
        pData[0]       = pBuffer[1];
        pData[1]       = pBuffer[2];
        pData[2]       = pBuffer[3];
        pData[3]       = pBuffer[4];
        pMsgpackDecode->nOffset += 5;
        *pValue = (double)fData;
    } break;
    case 0xcb: /* double */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }

        uint8_t* pData = (uint8_t*)pValue;
        pData[0]       = pBuffer[1];
        pData[1]       = pBuffer[2];
        pData[2]       = pBuffer[3];
        pData[3]       = pBuffer[4];
        pData[4]       = pBuffer[5];
        pData[5]       = pBuffer[6];
        pData[6]       = pBuffer[7];
        pData[7]       = pBuffer[8];
        pMsgpackDecode->nOffset += 9;
    } break;
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            *pValue = (double)(pBuffer[0]);
            pMsgpackDecode->nOffset += 1;
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            *pValue = (double)((int8_t)pBuffer[0]);
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    }
    }
    return true;
}

const char* msgpackDecode_readString(msgpackDecode_tt* pMsgpackDecode, int64_t* pLength)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        *pLength = -1;
        return "";
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xd9:
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }

        int64_t iLen = pBuffer[1];
        if (pMsgpackDecode->nOffset + 2 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 2 + iLen;
        *pLength                = iLen;
        return (const char*)pBuffer + 2;
    } break;
    case 0xda:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        int64_t iLen = (int64_t)((uint64_t)pBuffer[1] << 8 | (uint64_t)pBuffer[2]);
        if (pMsgpackDecode->nOffset + 3 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 3 + iLen;
        *pLength                = iLen;
        return (const char*)pBuffer + 3;
    } break;
    case 0xdb:
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        int64_t iLen = (int64_t)((uint64_t)pBuffer[1] << 24 | (uint64_t)pBuffer[2] << 16 |
                                 (uint64_t)pBuffer[3] << 8 | (uint64_t)pBuffer[4]);
        if (pMsgpackDecode->nOffset + 5 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }

        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 5 + iLen;
        *pLength                = iLen;
        return (const char*)pBuffer + 5;
    } break;
    default:
    {
        if ((pBuffer[0] & 0xe0) == 0xa0) /* fix raw */
        {
            int64_t iLen = pBuffer[0] & 0x1f;
            if (pMsgpackDecode->nOffset + 1 + iLen > pMsgpackDecode->nLength) {
                *pLength = -1;
                return "";
            }
            pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 1 + iLen;
            *pLength                = iLen;
            return (const char*)pBuffer + 1;
        }
    } break;
    }
    *pLength = -1;
    return "";
}

const char* msgpackDecode_readBinary(msgpackDecode_tt* pMsgpackDecode, int64_t* pLength)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        *pLength = -1;
        return "";
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xc4:
    {
        int64_t iLen = pBuffer[1];
        if (pMsgpackDecode->nOffset + 2 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 2 + iLen;
        *pLength                = iLen;
        return (const char*)pBuffer + 2;
    } break;
    case 0xc5:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        int64_t iLen = (int64_t)((uint64_t)pBuffer[1] << 8 | (uint64_t)pBuffer[2]);
        if (pMsgpackDecode->nOffset + 3 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 3 + iLen;
        *pLength                = iLen;
        return (const char*)pBuffer + 3;
    } break;
    case 0xc6:
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        int64_t iLen = (int64_t)((uint64_t)pBuffer[1] << 24 | (uint64_t)pBuffer[2] << 16 |
                                 (uint64_t)pBuffer[3] << 8 | (uint64_t)pBuffer[4]);
        if (pMsgpackDecode->nOffset + 5 + iLen > pMsgpackDecode->nLength) {
            *pLength = -1;
            return "";
        }
        *pLength                = iLen;
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 5 + iLen;
        return (const char*)pBuffer + 5;
    } break;
    }
    *pLength = -1;
    return "";
}

bool msgpackDecode_readArray(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xdc: /* array 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }

        *pValue = (int64_t)((uint64_t)pBuffer[1] << 8 | (uint64_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xdd: /* array 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }

        *pValue = (int64_t)((uint64_t)pBuffer[1] << 24 | (uint64_t)pBuffer[2] << 16 |
                            (uint64_t)pBuffer[3] << 8 | (uint64_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    default:
    {
        if ((pBuffer[0] & 0xf0) == 0x90) {
            *pValue = (int64_t)(pBuffer[0] & 0xf);
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    } break;
    }
    return true;
}

bool msgpackDecode_readMap(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xde: /* map 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }

        *pValue = (int64_t)((uint64_t)pBuffer[1] << 8 | (uint64_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xdf: /* map 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }

        *pValue = (int64_t)((uint64_t)pBuffer[1] << 24 | (uint64_t)pBuffer[2] << 16 |
                            (uint64_t)pBuffer[3] << 8 | (uint64_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    default:
    {
        if ((pBuffer[0] & 0xf0) == 0x80) {
            *pValue = (int64_t)(pBuffer[0] & 0xf);
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    } break;
    }
    return true;
}

bool msgpackDecode_readUserPointer(msgpackDecode_tt* pMsgpackDecode, void** ppValue)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
#ifdef DEF_PLATFORM_64BITS
    switch (flag) {
    case 0xc7:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }

        *ppValue = (void*)((uintptr_t)pBuffer[1] << 8 | (uintptr_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xc8: /* map 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }

        *ppValue = (void*)((uintptr_t)pBuffer[1] << 24 | (uintptr_t)pBuffer[2] << 16 |
                           (uintptr_t)pBuffer[3] << 8 | (uintptr_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xc9:
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        *ppValue = (void*)((uintptr_t)pBuffer[1] << 56 | (uintptr_t)pBuffer[2] << 48 |
                           (uintptr_t)pBuffer[3] << 40 | (uintptr_t)pBuffer[4] << 32 |
                           (uintptr_t)pBuffer[5] << 24 | (uintptr_t)pBuffer[6] << 16 |
                           (uintptr_t)pBuffer[7] << 8 | (uintptr_t)pBuffer[8]);
        pMsgpackDecode->nOffset += 9;
    } break;
    default:
    {
        return false;
    }
    }
#else
    switch (flag) {
    case 0xc7:
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }

        *ppValue = (void*)((uintptr_t)pBuffer[1]);
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xc8:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }

        *ppValue = (void*)((uintptr_t)pBuffer[1] << 8 | (uintptr_t)pBuffer[2]);
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xc9:
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        *ppValue = (void*)((uintptr_t)pBuffer[1] << 24 | (uintptr_t)pBuffer[2] << 16 |
                           (uintptr_t)pBuffer[3] << 8 | (uintptr_t)pBuffer[4]);
        pMsgpackDecode->nOffset += 5;
    } break;
    default:
    {
        return false;
    }
    }
#endif
    return true;
}

bool msgpackDecode_skipInteger(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    case 0xd0: /* int 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xcd: /* uint 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xd1: /* int 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xce: /* uint 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xd2: /* int 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xcf: /* uint 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 9;
    } break;
    case 0xd3: /* int 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 9;
    } break;
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            pMsgpackDecode->nOffset += 1;
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    }
    }
    return true;
}

bool msgpackDecode_skipBoolean(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xc3:
    {
        ++pMsgpackDecode->nOffset;
    } break;
    case 0xc2:
    {
        ++pMsgpackDecode->nOffset;
    } break;
    default:
    {
        return false;
    }
    }
    return true;
}

bool msgpackDecode_skipReal(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    case 0xd0: /* int 8 */
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 2;
    } break;
    case 0xcd: /* uint 16 */
    case 0xd1: /* int 16 */
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 3;
    } break;
    case 0xce: /* uint 32 */
    case 0xd2: /* int 32 */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xcf: /* uint 64 */
    case 0xd3: /* int 64 */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset += 9;
    } break;
    case 0xca: /* float */
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }

        pMsgpackDecode->nOffset += 5;
    } break;
    case 0xcb: /* double */
    {
        if (pMsgpackDecode->nOffset + 9 > pMsgpackDecode->nLength) {
            return false;
        }

        pMsgpackDecode->nOffset += 9;
    } break;
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            pMsgpackDecode->nOffset += 1;
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            pMsgpackDecode->nOffset += 1;
        }
        else {
            return false;
        }
    }
    }
    return true;
}

bool msgpackDecode_skipString(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xd9:
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }
        size_t nLen = (size_t)pBuffer[1];
        if (pMsgpackDecode->nOffset + 2 + nLen > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 2 + nLen;
    } break;
    case 0xda:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        size_t nLen = (size_t)pBuffer[1] << 8 | (size_t)pBuffer[2];
        if (pMsgpackDecode->nOffset + 3 + nLen > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 3 + nLen;
    } break;
    case 0xdb:
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        size_t nLen = (size_t)pBuffer[1] << 24 | (size_t)pBuffer[2] << 16 |
                      (size_t)pBuffer[3] << 8 | (size_t)pBuffer[4];
        if (pMsgpackDecode->nOffset + 5 + nLen > pMsgpackDecode->nLength) {
            return false;
        }

        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 5 + nLen;
    } break;
    default:
    {
        if ((pBuffer[0] & 0xe0) == 0xa0) /* fix raw */
        {
            size_t nLen = pBuffer[0] & 0x1f;
            if (pMsgpackDecode->nOffset + 1 + nLen > pMsgpackDecode->nLength) {
                return false;
            }
            pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 1 + nLen;
        }
        else {
            return false;
        }
    } break;
    }
    return true;
}

bool msgpackDecode_skipBinary(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xc4:
    {
        if (pMsgpackDecode->nOffset + 2 > pMsgpackDecode->nLength) {
            return false;
        }

        size_t nLen = (size_t)pBuffer[1];
        if (pMsgpackDecode->nOffset + 2 + nLen > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 2 + nLen;
    } break;
    case 0xc5:
    {
        if (pMsgpackDecode->nOffset + 3 > pMsgpackDecode->nLength) {
            return false;
        }
        size_t nLen = (size_t)pBuffer[1] << 8 | (size_t)pBuffer[2];
        if (pMsgpackDecode->nOffset + 3 + nLen > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 3 + nLen;
    } break;
    case 0xc6:
    {
        if (pMsgpackDecode->nOffset + 5 > pMsgpackDecode->nLength) {
            return false;
        }
        size_t nLen = (size_t)pBuffer[1] << 24 | (size_t)pBuffer[2] << 16 |
                      (size_t)pBuffer[3] << 8 | (size_t)pBuffer[4];
        if (pMsgpackDecode->nOffset + 5 + nLen > pMsgpackDecode->nLength) {
            return false;
        }
        pMsgpackDecode->nOffset = pMsgpackDecode->nOffset + 5 + nLen;
    } break;
    default:
    {
        return false;
    } break;
    }
    return true;
}

bool msgpackDecode_skipNil(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return false;
    }

    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    if (flag == 0xc0) {
        ++pMsgpackDecode->nOffset;
        return true;
    }
    return false;
}

bool msgpackDecode_isEnd(msgpackDecode_tt* pMsgpackDecode)
{
    return pMsgpackDecode->nLength == pMsgpackDecode->nOffset;
}

enMsgpackValueType msgpackDecode_getType(msgpackDecode_tt* pMsgpackDecode)
{
    if (pMsgpackDecode->nLength <= pMsgpackDecode->nOffset) {
        return eMsgpackInvalid;
    }
    const uint8_t* pBuffer = msgpackDecode_getBuffer(pMsgpackDecode);
    uint8_t        flag    = pBuffer[0];
    switch (flag) {
    case 0xcc: /* uint 8 */
    case 0xd0: /* int 8 */
    case 0xcd: /* uint 16 */
    case 0xd1: /* int 16 */
    case 0xce: /* uint 32 */
    case 0xd2: /* int 32 */
    case 0xcf: /* uint 64 */
    case 0xd3: /* int 64 */
    {
        return eMsgpackInteger;
    }
    case 0xc3:
    case 0xc2:
    {
        return eMsgpackBoolean;
    }
    case 0xca: /* float */
    case 0xcb: /* double */
    {
        return eMsgpackReal;
    }
    case 0xd9:
    case 0xda:
    case 0xdb:
    {
        return eMsgpackString;
    }
    case 0xc4:
    case 0xc5:
    case 0xc6:
    {
        return eMsgpackBinary;
    }
    case 0xc7:
    case 0xc8:
    case 0xc9:
    {
        return eMsgpackUserPointer;
    }
    case 0xdc: /* array 16 */
    case 0xdd: /* array 32 */
    {
        return eMsgpackArray;
    }
    case 0xde: /* map 16 */
    case 0xdf: /* map 32 */
    {
        return eMsgpackMap;
    }
    case 0xc0:
    {
        return eMsgpackNil;
    }
    default: /* types that can't be idenitified by first byte value. */
    {
        if ((pBuffer[0] & 0x80) == 0) /* positive fixnum */
        {
            return eMsgpackInteger;
        }
        else if ((pBuffer[0] & 0xe0) == 0xe0) /* negative fixnum */
        {
            return eMsgpackInteger;
        }
        else if ((pBuffer[0] & 0xe0) == 0xa0) /* fix raw */
        {
            return eMsgpackString;
        }
        else if ((pBuffer[0] & 0xf0) == 0x90) {
            return eMsgpackArray;
        }
        else if ((pBuffer[0] & 0xf0) == 0x80) {
            return eMsgpackMap;
        }
        else {
            return eMsgpackInvalid;
        }
    }
    }
}
