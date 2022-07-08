#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "platform_t.h"

typedef struct msgpackEncode_s
{
    void* (*fnExpand)(void*, size_t, size_t, void*);
    void*    pExpandData;
    uint8_t* pBuffer;
    size_t   nLength;
    size_t   nOffset;
} msgpackEncode_tt;

frCore_API void msgpackEncode_init(msgpackEncode_tt* pMsgpackEncode, void* pBuffer, size_t nLength,
                                   void* (*fnExpand)(void*, size_t, size_t, void*),
                                   void* pExpandData);

frCore_API void msgpackEncode_clear(msgpackEncode_tt* pMsgpackEncode);

frCore_API void msgpackEncode_writeInteger(msgpackEncode_tt* pMsgpackEncode, int64_t iValue);

frCore_API void msgpackEncode_writeBoolean(msgpackEncode_tt* pMsgpackEncode, bool bValue);

frCore_API void msgpackEncode_writeReal(msgpackEncode_tt* pMsgpackEncode, double fValue);

frCore_API void msgpackEncode_writeString(msgpackEncode_tt* pMsgpackEncode, const char* szValue,
                                          size_t nLength);

frCore_API void msgpackEncode_writeBinary(msgpackEncode_tt* pMsgpackEncode, const char* pValue,
                                          size_t nLength);

frCore_API void msgpackEncode_writeArray(msgpackEncode_tt* pMsgpackEncode, int64_t iValue);

frCore_API void msgpackEncode_writeMap(msgpackEncode_tt* pMsgpackEncode, int64_t iValue);

frCore_API void msgpackEncode_writeUserPointer(msgpackEncode_tt* pMsgpackEncode, void* pValue);

frCore_API void msgpackEncode_writeNil(msgpackEncode_tt* pMsgpackEncode);

frCore_API void msgpackEncode_swap(msgpackEncode_tt* pMsgpackEncode, void** ppBuffer,
                                   size_t* pLength, size_t* pOffset);

static inline size_t msgpackEncode_getWritten(msgpackEncode_tt* pMsgpackEncode)
{
    return pMsgpackEncode->nOffset;
}