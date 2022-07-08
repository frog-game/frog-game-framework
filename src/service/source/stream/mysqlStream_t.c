

#include "stream/mysqlStream_t.h"

#include <stdatomic.h>

#include "byteQueue_t.h"
#include "slice_t.h"

#include "channel/channel_t.h"
#include "internal/service-inl.h"
#include "stream/codecStream_t.h"

typedef struct mysqlStream_s
{
    codecStream_tt codec;
    uint8_t        uiSeqid;
    atomic_int     iRefCount;
} mysqlStream_tt;

#define MAX_MYSQL_PAYLOAD ((1 << 24) - 1)

static inline int32_t mysql_encodeBufCount(const char* pBuffer, size_t nLength)
{
    return ((MAX_MYSQL_PAYLOAD - 1) + 0xfffe) / MAX_MYSQL_PAYLOAD;
}

static inline int32_t mysql_encodeVecBufCount(ioBufVec_tt* pInBufVec, int32_t iCount)
{
    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }

    return (nLength + (MAX_MYSQL_PAYLOAD - 1)) / MAX_MYSQL_PAYLOAD;
}

static inline int32_t mysql_encode(const char* pBuffer, int32_t iLength, uint8_t uiSeqid,
                                   ioBufVec_tt* pOutBufVec)
{
    assert(pBuffer && pOutBufVec && iLength > 0);

    int32_t iEncodeBufCount = 0;

    int32_t iFragmentCount = (iLength + (MAX_MYSQL_PAYLOAD - 1)) / MAX_MYSQL_PAYLOAD;
    if (iFragmentCount == 1) {
        uint8_t* pPackBuffer = mem_malloc(sizeof(uint32_t) + iLength);
        pPackBuffer[0]       = iLength & 0xFF;
        pPackBuffer[1]       = (iLength >> 8) & 0xFF;
        pPackBuffer[2]       = (iLength >> 16) & 0xFF;
        pPackBuffer[3]       = uiSeqid;
        memcpy(pPackBuffer + sizeof(uint32_t), pBuffer, iLength);
        pOutBufVec[iEncodeBufCount].iLength = iLength + sizeof(uint32_t);
        pOutBufVec[iEncodeBufCount].pBuf    = (char*)pPackBuffer;
        ++iEncodeBufCount;
    }
    else {
        for (int32_t j = 0; j < iFragmentCount; j++) {
            size_t nPackLength =
                j == iFragmentCount - 1 ? iLength - j * MAX_MYSQL_PAYLOAD : MAX_MYSQL_PAYLOAD;
            uint8_t* pPackBuffer = mem_malloc(sizeof(uint32_t) + nPackLength);
            pPackBuffer[0]       = nPackLength & 0xFF;
            pPackBuffer[1]       = (nPackLength >> 8) & 0xFF;
            pPackBuffer[2]       = (nPackLength >> 16) & 0xFF;
            pPackBuffer[3]       = uiSeqid;
            memcpy(pPackBuffer + sizeof(uint32_t), pBuffer, nPackLength);
            pOutBufVec[iEncodeBufCount].iLength = nPackLength + sizeof(uint32_t);
            pOutBufVec[iEncodeBufCount].pBuf    = (char*)pPackBuffer;
            ++iEncodeBufCount;
            ++uiSeqid;
        }
    }
    return iEncodeBufCount;
}

int32_t mysql_encodeVec(ioBufVec_tt* pInBufVec, int32_t iCount, uint8_t uiSeqid,
                        ioBufVec_tt* pOutBufVec)
{
    assert(pInBufVec && pOutBufVec && (iCount > 0));

    int32_t iEncodeBufCount = 0;

    size_t nLength = 0;
    for (int32_t i = 0; i < iCount; i++) {
        nLength += pInBufVec[i].iLength;
    }

    int32_t iFragmentCount = (nLength + (MAX_MYSQL_PAYLOAD - 1)) / MAX_MYSQL_PAYLOAD;
    if (iFragmentCount == 1) {
        uint8_t* pPackBuffer = mem_malloc(sizeof(uint32_t) + nLength);
        pPackBuffer[0]       = nLength & 0xFF;
        pPackBuffer[1]       = (nLength >> 8) & 0xFF;
        pPackBuffer[2]       = (nLength >> 16) & 0xFF;
        pPackBuffer[3]       = uiSeqid;

        size_t nOffset = sizeof(uint32_t);

        for (int32_t i = 0; i < iCount; i++) {
            memcpy(pPackBuffer + nOffset, pInBufVec[i].pBuf, pInBufVec[i].iLength);
            nOffset += pInBufVec[i].iLength;
        }
        pOutBufVec[iEncodeBufCount].iLength = nLength + sizeof(uint32_t);
        pOutBufVec[iEncodeBufCount].pBuf    = (char*)pPackBuffer;
        ++iEncodeBufCount;
    }
    else {
        int32_t iSrcBufIndex  = 0;
        size_t  nSrcBufOffset = 0;

        for (int32_t j = 0; j < iFragmentCount; j++) {
            size_t nPackLength =
                j == iFragmentCount - 1 ? nLength - j * MAX_MYSQL_PAYLOAD : MAX_MYSQL_PAYLOAD;
            uint8_t* pPackBuffer = mem_malloc(sizeof(uint32_t) + nPackLength);
            pPackBuffer[0]       = nPackLength & 0xFF;
            pPackBuffer[1]       = (nPackLength >> 8) & 0xFF;
            pPackBuffer[2]       = (nPackLength >> 16) & 0xFF;
            pPackBuffer[3]       = uiSeqid;

            pOutBufVec[iEncodeBufCount].iLength = sizeof(uint32_t) + nPackLength;

            size_t nOffset = sizeof(uint32_t);
            for (;;) {
                if (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset >
                    pOutBufVec[iEncodeBufCount].iLength - nOffset) {
                    memcpy(pPackBuffer + nOffset,
                           pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                           pOutBufVec[iEncodeBufCount].iLength - nOffset);
                    nSrcBufOffset += (pOutBufVec[iEncodeBufCount].iLength - nOffset);
                    break;
                }
                else {
                    memcpy(pPackBuffer + nOffset,
                           pInBufVec[iSrcBufIndex].pBuf + nSrcBufOffset,
                           pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                    nOffset += (pInBufVec[iSrcBufIndex].iLength - nSrcBufOffset);
                    nSrcBufOffset = 0;
                    ++iSrcBufIndex;
                    if (nOffset == pOutBufVec[iEncodeBufCount].iLength) {
                        break;
                    }
                }
            }
            pOutBufVec[iEncodeBufCount].pBuf = (char*)pPackBuffer;
            ++iEncodeBufCount;
            ++uiSeqid;
        }
    }
    return iEncodeBufCount;
}

static int32_t mysqlStream_write(codecStream_tt*           pHandle,
                                 struct eventConnection_s* pEventConnection, const char* pBuffer,
                                 int32_t iLength, uint32_t uiFlag, uint32_t uiToken)
{
    mysqlStream_tt* pStream   = container_of(pHandle, mysqlStream_tt, codec);
    const int32_t   iBufCount = mysql_encodeBufCount(pBuffer, iLength);
    ioBufVec_tt     bufVec[iBufCount];
    mysql_encode(pBuffer, iLength, pStream->uiSeqid, bufVec);
    pStream->uiSeqid = 0;
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iBufCount, NULL, 0));
}

static int32_t mysqlStream_writeMove(codecStream_tt*           pHandle,
                                     struct eventConnection_s* pEventConnection,
                                     ioBufVec_tt* pInBufVec, int32_t iCount, uint32_t uiFlag,
                                     uint32_t uiToken)
{
    mysqlStream_tt* pStream   = container_of(pHandle, mysqlStream_tt, codec);
    const int32_t   iBufCount = mysql_encodeVecBufCount(pInBufVec, iCount);
    ioBufVec_tt     bufVec[iBufCount];
    mysql_encodeVec(pInBufVec, iCount, pStream->uiSeqid, bufVec);
    pStream->uiSeqid = 0;
    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iCount, NULL, 0));
}

static void mysqlStream_addref(codecStream_tt* pHandle)
{
    mysqlStream_tt* pStream = container_of(pHandle, mysqlStream_tt, codec);
    atomic_fetch_add(&pStream->iRefCount, 1);
}

static void mysqlStream_release(codecStream_tt* pHandle)
{
    mysqlStream_tt* pStream = container_of(pHandle, mysqlStream_tt, codec);
    if (atomic_fetch_sub(&pStream->iRefCount, 1) == 1) {
        mem_free(pStream);
    }
}

codecStream_tt* mysqlStreamCreate()
{
    mysqlStream_tt* pHandle = mem_malloc(sizeof(mysqlStream_tt));
    atomic_init(&pHandle->iRefCount, 1);
    pHandle->uiSeqid           = 1;
    pHandle->codec.fnReceive   = NULL;
    pHandle->codec.fnWrite     = mysqlStream_write;
    pHandle->codec.fnWriteMove = mysqlStream_writeMove;
    pHandle->codec.fnAddref    = mysqlStream_addref;
    pHandle->codec.fnRelease   = mysqlStream_release;
    return &pHandle->codec;
}
