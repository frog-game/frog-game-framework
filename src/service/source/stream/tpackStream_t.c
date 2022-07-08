

#include "stream/tpackStream_t.h"

#include <stdatomic.h>

#include "byteQueue_t.h"
#include "codec/tpack_t.h"
#include "slice_t.h"

#include "channel/channel_t.h"
#include "internal/service-inl.h"
#include "stream/codecStream_t.h"

typedef struct tpackStream_s
{
    codecStream_tt codec;
    atomic_int     iRefCount;
} tpackStream_tt;

static int32_t tpackStream_write(codecStream_tt*           pHandle,
                                 struct eventConnection_s* pEventConnection, const char* pBuffer,
                                 int32_t iLength, uint32_t uiFlag, uint32_t uiToken)
{
    const int32_t iBufCount = tpack_encodeBufCount(pBuffer, iLength);
    ioBufVec_tt   bufVec[iBufCount];
    tpack_encode(pBuffer, iLength, (uint8_t)uiFlag, uiToken, bufVec);
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iBufCount, NULL, 0));
}

static int32_t tpackStream_writeMove(codecStream_tt*           pHandle,
                                     struct eventConnection_s* pEventConnection,
                                     ioBufVec_tt* pInBufVec, int32_t iCount, uint32_t uiFlag,
                                     uint32_t uiToken)
{
    const int32_t iBufCount = tpack_encodeVecBufCount(pInBufVec, iCount);
    ioBufVec_tt   bufVec[iBufCount];
    tpack_encodeVec(pInBufVec, iCount, (uint8_t)uiFlag, uiToken, bufVec);
    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iCount, NULL, 0));
}

static bool tpackStream_receive(codecStream_tt* pHandle, channel_tt* pChannel,
                                byteQueue_tt* pReadByteQueue)
{
    tpackHead_tt head;
    uint8_t      szToken[4];

    while (byteQueue_getBytesReadable(pReadByteQueue) >= 2) {
        int32_t r = tpack_decodeHead(pReadByteQueue, &head);
        if (r == 0) {
            return true;
        }
        else if (r == -1) {
            return false;
        }
        else {
            if ((head.uiFlag & DEF_TP_FRAME_FINAL) && (head.uiFlag != DEF_TP_FRAME_FINAL)) {
                uint32_t uiToken = 0;

                if (head.uiOffset > 4) {
                    byteQueue_readOffset(pReadByteQueue, head.uiOffset - 4);
                    byteQueue_readBytes(pReadByteQueue, szToken, 4, false);
                    uiToken = ((uint32_t)szToken[0] << 24) | ((uint32_t)szToken[1] << 16) |
                              ((uint32_t)szToken[2] << 8) | (uint32_t)szToken[3];
                }
                else {
                    byteQueue_readOffset(pReadByteQueue, head.uiOffset);
                }

                serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + head.uiPayloadLen);
                pEvent->uiSourceID      = channel_getID(pChannel);
                pEvent->uiToken         = uiToken;
                if (head.uiPayloadLen > 0) {
                    byteQueue_readBytes(
                        pReadByteQueue, pEvent->szStorage, head.uiPayloadLen, false);
                }
                pEvent->uiLength =
                    head.uiPayloadLen | (((head.uiFlag & DEF_TP_CODE_MASK) | DEF_EVENT_MSG) << 24);
                if (!service_enqueue(channel_getService(pChannel), pEvent)) {
                    return false;
                }
            }
            else {
                uint32_t         uiEventBufferLength = head.uiOffset + head.uiPayloadLen;
                serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + uiEventBufferLength);
                pEvent->uiSourceID      = channel_getID(pChannel);
                pEvent->uiToken         = 0;
                if (uiEventBufferLength > 0) {
                    byteQueue_readBytes(
                        pReadByteQueue, pEvent->szStorage, uiEventBufferLength, false);
                }
                pEvent->uiLength = uiEventBufferLength | (DEF_EVENT_BINARY << 24);
                if (!service_enqueue(channel_getService(pChannel), pEvent)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static void tpackStream_addref(codecStream_tt* pHandle)
{
    tpackStream_tt* pStream = container_of(pHandle, tpackStream_tt, codec);
    atomic_fetch_add(&pStream->iRefCount, 1);
}

static void tpackStream_release(codecStream_tt* pHandle)
{
    tpackStream_tt* pStream = container_of(pHandle, tpackStream_tt, codec);
    if (atomic_fetch_sub(&pStream->iRefCount, 1) == 1) {
        mem_free(pStream);
    }
}

codecStream_tt* tpackStreamCreate()
{
    tpackStream_tt* pHandle = mem_malloc(sizeof(tpackStream_tt));
    atomic_init(&pHandle->iRefCount, 1);
    pHandle->codec.fnReceive   = tpackStream_receive;
    pHandle->codec.fnWrite     = tpackStream_write;
    pHandle->codec.fnWriteMove = tpackStream_writeMove;
    pHandle->codec.fnAddref    = tpackStream_addref;
    pHandle->codec.fnRelease   = tpackStream_release;
    return &pHandle->codec;
}
