#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "platform_t.h"

typedef enum
{
    eMsgpackInteger,
    eMsgpackBoolean,
    eMsgpackReal,
    eMsgpackString,
    eMsgpackBinary,
    eMsgpackArray,
    eMsgpackMap,
    eMsgpackNil,
    eMsgpackUserPointer,
    eMsgpackInvalid
} enMsgpackValueType;

typedef struct msgpackDecode_s
{
    const uint8_t* pBuffer;
    size_t         nLength;
    size_t         nOffset;
} msgpackDecode_tt;

frCore_API void msgpackDecode_init(msgpackDecode_tt* pMsgpackDecode, const uint8_t* pBuffer,
                                   size_t nLength, size_t nOffset);

frCore_API void msgpackDecode_clear(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_peekInteger(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue);

frCore_API bool msgpackDecode_readInteger(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue);

frCore_API bool msgpackDecode_readBoolean(msgpackDecode_tt* pMsgpackDecode, bool* pValue);

frCore_API bool msgpackDecode_readReal(msgpackDecode_tt* pMsgpackDecode, double* pValue);

frCore_API const char* msgpackDecode_readString(msgpackDecode_tt* pMsgpackDecode, int64_t* pLength);

frCore_API const char* msgpackDecode_readBinary(msgpackDecode_tt* pMsgpackDecode, int64_t* pLength);

frCore_API bool msgpackDecode_readArray(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue);

frCore_API bool msgpackDecode_readMap(msgpackDecode_tt* pMsgpackDecode, int64_t* pValue);

frCore_API bool msgpackDecode_readUserPointer(msgpackDecode_tt* pMsgpackDecode, void** ppValue);

frCore_API bool msgpackDecode_skipInteger(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_skipBoolean(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_skipReal(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_skipString(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_skipBinary(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_skipNil(msgpackDecode_tt* pMsgpackDecode);

frCore_API bool msgpackDecode_isEnd(msgpackDecode_tt* pMsgpackDecode);

frCore_API enMsgpackValueType msgpackDecode_getType(msgpackDecode_tt* pMsgpackDecode);
