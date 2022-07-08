#pragma once

#include "byteQueue_t.h"
#include "cbuf_t.h"
#include "utility_t.h"

typedef struct lockStepPackHead_s
{
    uint8_t  packLength;
    uint16_t frameId;
} lockStepPackHead_tt;

__UNUSED int32_t lockStepPack_decodeHead(byteQueue_tt* pInBytes, lockStepPackHead_tt* pOutHead);

__UNUSED int32_t lockStepPack_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag,
                                     uint32_t* pOutToken);

__UNUSED int32_t lockStepPack_encode(const char* pBuffer, size_t nLength, uint8_t uiFlag,
                                     uint32_t uiToken, ioBufVec_tt* pOutBufVec);

__UNUSED int32_t lockStepPack_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiFlag,
                                        uint32_t uiToken, ioBufVec_tt* pOutBufVec);

static inline int32_t lockStepPack_encodeBufCount(const char* pBuffer, size_t nLength)
{
    return (int32_t)((nLength + 0xfffe) / 0xffff);
}

static inline int32_t lockStepPack_encodeVecBufCount(ioBufVec_tt* pInBufVec, int32_t iCount)
{
    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }
    return (int32_t)((nLength + 0xfffe) / 0xffff);
}
