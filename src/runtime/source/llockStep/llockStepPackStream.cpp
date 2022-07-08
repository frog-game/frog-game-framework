
#include <atomic>
#include <malloc.h>

#include "llockStepPackStream.h"

extern "C" {
#include "byteQueue_t.h"
#include "channel/channel_t.h"
#include "llockStepPack.h"
#include "slice_t.h"
#include "stream/codecStream_t.h"
}

#if defined(__clang__)
#    define def_VariableLengthArrays
#elif defined(__GNUC__) || defined(__GNUG__)
#    define def_VariableLengthArrays
#endif

typedef struct lockStepPackStream_s
{
    codecStream_tt  codec;
    std::atomic_int iRefCount;
} lockStepPackStream_tt;

static int32_t lockStepPackStream_write(codecStream_tt*           pHandle,
                                        struct eventConnection_s* pEventConnection,
                                        const char* pBuffer, int32_t iLength, uint32_t uiFlag,
                                        uint32_t uiToken)
{
    const int32_t iBufCount = lockStepPack_encodeBufCount(pBuffer, iLength);
#ifdef def_VariableLengthArrays
    ioBufVec_tt bufVec[iBufCount];
#else
    ioBufVec_tt* bufVec = (ioBufVec_tt*)_alloca(sizeof(ioBufVec_tt) * iBufCount);
#endif

    lockStepPack_encode(pBuffer, iLength, (uint8_t)uiFlag, uiToken, bufVec);
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iBufCount, NULL, 0));
}

static int32_t lockStepPackStream_writeMove(codecStream_tt*           pHandle,
                                            struct eventConnection_s* pEventConnection,
                                            ioBufVec_tt* pInBufVec, int32_t iCount, uint32_t uiFlag,
                                            uint32_t uiToken)
{
    const int32_t iBufCount = lockStepPack_encodeVecBufCount(pInBufVec, iCount);
#ifdef def_VariableLengthArrays
    ioBufVec_tt bufVec[iBufCount];
#else
    ioBufVec_tt* bufVec = (ioBufVec_tt*)_alloca(sizeof(ioBufVec_tt) * iBufCount);
#endif

    lockStepPack_encodeVec(pInBufVec, iCount, (uint8_t)uiFlag, uiToken, bufVec);
    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return eventConnection_send(pEventConnection, createEventBuf_move(bufVec, iCount, NULL, 0));
}

static bool lockStepPackStream_receive(codecStream_tt* pHandle, channel_tt* pChannel,
                                       byteQueue_tt* pReadByteQueue)
{
    lockStepPackHead_tt head;
    while (byteQueue_getBytesReadable(pReadByteQueue) >= 2) {
        int32_t r = lockStepPack_decodeHead(pReadByteQueue, &head);
        if (r == 0) {
            return true;
        }
        else if (r == -1) {
            return false;
        }
        else {
            if ((head.uiFlag & DEF_TP_FRAME_FINAL) && (head.uiFlag != DEF_TP_FRAME_FINAL)) {
                byteQueue_readOffset(pReadByteQueue, head.uiOffset);
                if (!channel_pushService(pChannel,
                                         pReadByteQueue,
                                         head.uiPayloadLen,
                                         (DEF_EVENT_MSG_SEND | DEF_EVENT_MSG),
                                         0)) {
                    return false;
                }
            }
            else {
                uint32_t uiEventBufferLength = head.uiOffset + head.uiPayloadLen;
                if (!channel_pushService(
                        pChannel, pReadByteQueue, uiEventBufferLength, DEF_EVENT_BINARY, 0)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static void lockStepPackStream_addref(codecStream_tt* pHandle)
{
    lockStepPackStream_tt* pStream = container_of(pHandle, lockStepPackStream_tt, codec);
    atomic_fetch_add(&pStream->iRefCount, 1);
}

static void lockStepPackStream_release(codecStream_tt* pHandle)
{
    lockStepPackStream_tt* pStream = container_of(pHandle, lockStepPackStream_tt, codec);
    if (atomic_fetch_sub(&pStream->iRefCount, 1) == 1) {
        mem_free(pStream);
    }
}

codecStream_tt* lockStepPackStreamCreate()
{
    lockStepPackStream_tt* pHandle =
        (lockStepPackStream_tt*)mem_malloc(sizeof(lockStepPackStream_tt));
    atomic_init(&pHandle->iRefCount, 1);
    pHandle->codec.fnReceive   = lockStepPackStream_receive;
    pHandle->codec.fnWrite     = lockStepPackStream_write;
    pHandle->codec.fnWriteMove = lockStepPackStream_writeMove;
    pHandle->codec.fnAddref    = lockStepPackStream_addref;
    pHandle->codec.fnRelease   = lockStepPackStream_release;
    return &pHandle->codec;
}
