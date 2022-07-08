#pragma once

#include "byteQueue_t.h"
#include "cbuf_t.h"


#define DEF_WS_FRAME_FINAL 0x80
#define DEF_WS_FRAME_PING 0x50
#define DEF_WS_FRAME_PONG 0x40
#define DEF_WS_FRAME_CLOSE 0x30
#define DEF_WS_FRAME_BINARY 0x20
#define DEF_WS_FRAME_TEXT 0x10

#define DEF_WS_CODE_MASK 0x70
#define DEF_WS_FRAME_MASK 0xF0

typedef struct webSocketHead_s
{
    uint16_t uiPayloadLen;
    uint8_t  uiFlag;
    uint8_t  uiOffset;
} webSocketHead_tt;

frCore_API int32_t webSocket_decodeHead(byteQueue_tt* pInBytes, webSocketHead_tt* pOutHead);

frCore_API int32_t webSocket_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag);

frCore_API int32_t webSocket_encode(const char* pBuffer, size_t nLength, uint8_t uiFlag,
                                    ioBufVec_tt* pOutBufVec);

frCore_API int32_t webSocket_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiFlag,
                                       ioBufVec_tt* pOutBufVec);

static inline int32_t webSocket_encodeBufCount(const char* pBuffer, size_t nLength)
{
    return nLength / 0xffff + 1;
}

static inline int32_t webSocket_encodeVecBufCount(ioBufVec_tt* pInBufVec, int32_t iCount)
{
    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }
    return nLength / 0xffff + 1;
}
