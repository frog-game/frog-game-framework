

#include "stream/webSocketStream_t.h"

#include "byteQueue_t.h"
#include "codec/webSocket_t.h"
#include "slice_t.h"

#include "channel/channel_t.h"
#include "internal/service-inl.h"
#include "stream/codecStream_t.h"

typedef struct webSocketStream_S
{
    codecStream_tt codec;
    atomic_int     iRefCount;
} webSocketStream_tt;

static inline void unMask(char* pBuffer, size_t nLength, char szMask[4])
{
    for (size_t i = 0; i < nLength; ++i) {
        pBuffer[i] ^= szMask[i % 4];
    }
}

static int32_t webSocketStream_write(codecStream_tt*           pHandle,
                                     struct eventConnection_s* pEventConnection,
                                     const char* pBuffer, int32_t iLength, uint32_t uiFlag,
                                     uint32_t uiToken)
{
    const int32_t iBufCount = webSocket_encodeBufCount(pBuffer, iLength);
    ioBufVec_tt   bufVec[iBufCount];
    webSocket_encode(pBuffer, iLength, (uint8_t)uiFlag, bufVec);
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iBufCount, NULL, 0));
}

static int32_t webSocketStream_writeMove(codecStream_tt*           pHandle,
                                         struct eventConnection_s* pEventConnection,
                                         ioBufVec_tt* pInBufVec, int32_t iCount, uint32_t uiFlag,
                                         uint32_t uiToken)
{
    const int32_t iBufCount = webSocket_encodeVecBufCount(pInBufVec, iCount);
    ioBufVec_tt   bufVec[iBufCount];
    webSocket_encodeVec(pInBufVec, iCount, (uint8_t)uiFlag, bufVec);
    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iCount, NULL, 0));
}

static bool webSocketStream_receive(codecStream_tt* pHandle, channel_tt* pChannel,
                                    byteQueue_tt* pReadByteQueue)
{
    webSocketHead_tt head;
    char             szMask[4];

    while (byteQueue_getBytesReadable(pReadByteQueue) >= 2) {
        int32_t r = webSocket_decodeHead(pReadByteQueue, &head);
        if (r == 0) {
            return true;
        }
        else if (r == -1) {
            return false;
        }
        else {
            if ((head.uiFlag & DEF_WS_FRAME_FINAL) && (head.uiFlag != DEF_WS_FRAME_FINAL)) {
                if (head.uiOffset > 4) {
                    byteQueue_readOffset(pReadByteQueue, head.uiOffset - 4);
                    byteQueue_readBytes(pReadByteQueue, szMask, 4, false);
                }
                else {
                    byteQueue_readOffset(pReadByteQueue, head.uiOffset);
                }

                serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + head.uiPayloadLen);
                pEvent->uiSourceID      = channel_getID(pChannel);
                pEvent->uiToken         = 0;
                if (head.uiPayloadLen > 0) {
                    byteQueue_readBytes(
                        pReadByteQueue, pEvent->szStorage, head.uiPayloadLen, false);
                    if (head.uiOffset > 4) {
                        unMask(pEvent->szStorage, head.uiPayloadLen, szMask);
                    }
                }
                pEvent->uiLength =
                    head.uiPayloadLen | (((head.uiFlag & DEF_WS_CODE_MASK) | DEF_EVENT_MSG) << 24);
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

static void webSocketStream_addref(codecStream_tt* pHandle)
{
    webSocketStream_tt* pStream = container_of(pHandle, webSocketStream_tt, codec);
    atomic_fetch_add(&pStream->iRefCount, 1);
}

static void webSocketStream_release(codecStream_tt* pHandle)
{
    webSocketStream_tt* pStream = container_of(pHandle, webSocketStream_tt, codec);
    if (atomic_fetch_sub(&pStream->iRefCount, 1) == 1) {
        mem_free(pStream);
    }
}

codecStream_tt* webSocketStreamCreate()
{
    webSocketStream_tt* pHandle = mem_malloc(sizeof(webSocketStream_tt));
    atomic_init(&pHandle->iRefCount, 1);
    pHandle->codec.fnReceive   = webSocketStream_receive;
    pHandle->codec.fnWrite     = webSocketStream_write;
    pHandle->codec.fnWriteMove = webSocketStream_writeMove;
    pHandle->codec.fnAddref    = webSocketStream_addref;
    pHandle->codec.fnRelease   = webSocketStream_release;
    return &pHandle->codec;
}
