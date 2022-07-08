#pragma once

#include "byteQueue_t.h"
#include "cbuf_t.h"


#define DEF_TP_FRAME_FINAL 0x80
#define DEF_TP_FRAME_REPLY 0x70
#define DEF_TP_FRAME_CALL 0x60
#define DEF_TP_FRAME_PING 0x50
#define DEF_TP_FRAME_PONG 0x40
#define DEF_TP_FRAME_CLOSE 0x30
#define DEF_TP_FRAME_BINARY 0x20
#define DEF_TP_FRAME_TEXT 0x10

#define DEF_TP_CODE_MASK 0x70
#define DEF_TP_FRAME_MASK 0xF0

typedef struct tpackHead_s
{
    uint16_t uiPayloadLen;
    uint8_t  uiFlag;
    uint8_t  uiOffset;
} tpackHead_tt;

frCore_API int32_t tpack_decodeHead(byteQueue_tt* pInBytes, tpackHead_tt* pOutHead);

frCore_API int32_t tpack_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag,
                                uint32_t* pOutToken);

frCore_API int32_t tpack_encode(const char* pBuffer, size_t nLength, uint8_t uiFlag,
                                uint32_t uiToken, ioBufVec_tt* pOutBufVec);

frCore_API int32_t tpack_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiFlag,
                                   uint32_t uiToken, ioBufVec_tt* pOutBufVec);

static inline int32_t tpack_encodeBufCount(const char* pBuffer, size_t nLength)
{
    return nLength / 0xffff + 1;
}

static inline int32_t tpack_encodeVecBufCount(ioBufVec_tt* pInBufVec, int32_t iCount)
{
    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }
    return nLength / 0xffff + 1;
}
