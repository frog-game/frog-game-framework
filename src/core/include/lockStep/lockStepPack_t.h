#pragma once

#include "byteQueue_t.h"
#include "cbuf_t.h"
#include "utility_t.h"


typedef struct lockStepPackHead_s
{
    uint8_t  packLength;
    uint16_t frameId;
} lockStepPackHead_tt;

frCore_API int32_t lockStepPack_decodeHead(byteQueue_tt* pInBytes, lockStepPackHead_tt* pOutHead);

frCore_API int32_t lockStepPack_decode(byteQueue_tt* pInBytes, cbuf_tt* pOutBuf, uint8_t* pOutFlag,
                                       uint32_t* pOutToken);

frCore_API int32_t lockStepPack_encode(const char* pBuffer, size_t nLength,
                                       ioBufVec_tt* pOutBufVec);

frCore_API int32_t lockStepPack_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount,
                                          ioBufVec_tt* pOutBufVec);

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
