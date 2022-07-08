

#include "channel/channel_t.h"

#include "eventIO/eventIO_t.h"

#include "channel/channelCenter_t.h"

#include "serviceEvent_t.h"
#include "service_t.h"

#include "internal/service-inl.h"

#include "stream/codecStream_t.h"

#include <stdatomic.h>
#include <stdlib.h>

enum enStatus
{
    eStarting = 0,
    eRunning  = 1,
    eStopping = 2,
    eStopped  = 3,
};

struct channel_s
{
    eventIO_tt*                  pEventIO;
    codecStream_tt*              pCodecStream;
    service_tt*                  pService;
    uint32_t                     uiID;
    _Atomic(eventConnection_tt*) hConnection;
    _Atomic(eventTimer_tt*)      hDisconnectTimeout;
    atomic_int                   iStatus;
    atomic_int                   iRefCount;
};

static inline void eventConnection_onUserFree(void* pUserData)
{
    channel_tt* pChannel = (channel_tt*)pUserData;
    if (pChannel->uiID != 0) {
        channelCenter_deregister(pChannel->uiID);
        pChannel->uiID = 0;
    }
    channel_release(pChannel);
}

static inline void eventTimer_onClose(eventTimer_tt* pHandle, void* pData)
{
    channel_tt* pChannel = (channel_tt*)pData;
    channel_release(pChannel);
}

static inline void eventTimer_onDisconnectTimeout(eventTimer_tt* pHandle, void* pData)
{
    channel_tt* pChannel = (channel_tt*)pData;

    eventTimer_tt* pDisconnectTimeoutHandle =
        (eventTimer_tt*)atomic_exchange(&pChannel->hDisconnectTimeout, 0);
    if (pDisconnectTimeoutHandle) {
        eventTimer_release(pDisconnectTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pChannel->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);
    }

    channel_release(pChannel);
}

static inline void eventConnection_onClose(eventConnection_tt* pHandle, void* pData)
{
    channel_tt*         pChannel = (channel_tt*)pData;
    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pChannel->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_release(pConnectionHandle);
    }

    eventTimer_tt* pDisconnectTimeoutHandle =
        (eventTimer_tt*)atomic_exchange(&pChannel->hDisconnectTimeout, 0);
    if (pDisconnectTimeoutHandle) {
        eventTimer_stop(pDisconnectTimeoutHandle);
        eventTimer_release(pDisconnectTimeoutHandle);
    }

    channel_release(pChannel);
}

static inline bool eventConnection_onReceiveCallback(eventConnection_tt* pEventConnection,
                                                     byteQueue_tt* pReadByteQueue, void* pData)
{
    channel_tt* pChannel = (channel_tt*)pData;
    if (pChannel->pCodecStream && pChannel->pCodecStream->fnReceive) {
        return pChannel->pCodecStream->fnReceive(pChannel->pCodecStream, pChannel, pReadByteQueue);
    }
    else {
        size_t           nBytesWritten = byteQueue_getBytesReadable(pReadByteQueue);
        serviceEvent_tt* pEvent        = mem_malloc(sizeof(serviceEvent_tt) + nBytesWritten);
        pEvent->uiSourceID             = channel_getID(pChannel);
        pEvent->uiToken                = 0;
        pEvent->uiLength               = nBytesWritten | (DEF_EVENT_BINARY << 24);
        byteQueue_readBytes(pReadByteQueue, pEvent->szStorage, nBytesWritten, false);
        return service_enqueue(pChannel->pService, pEvent);
    }
}

static inline void eventConnection_onSendCompleteCallback(eventConnection_tt* pEventConnection,
                                                          void* pUserData, bool bSendComplete,
                                                          uintptr_t uiWriteUser)
{
    channel_tt* pChannel = (channel_tt*)pUserData;

    serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt));
    if (bSendComplete) {
        pEvent->uiLength = DEF_EVENT_SEND_OK << 24;
    }
    else {
        pEvent->uiLength = (DEF_EVENT_MSG | DEF_EVENT_MSG_CLOSE) << 24;
    }
    pEvent->uiSourceID = pChannel->uiID;
    pEvent->uiToken    = (uint32_t)uiWriteUser;
    service_enqueue(pChannel->pService, pEvent);
}

static inline void eventConnection_onDisconnectCallback(eventConnection_tt* pEventConnection,
                                                        void*               pData)
{
    channel_tt* pChannel = (channel_tt*)pData;

    serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt));
    pEvent->uiLength        = DEF_EVENT_DISCONNECT << 24;
    pEvent->uiSourceID      = pChannel->uiID;
    pEvent->uiToken         = 0;
    service_enqueue(pChannel->pService, pEvent);
}

channel_tt* createChannel(eventIO_tt* pEventIO, eventConnection_tt* pEventConnection)
{
    channel_tt* pHandle   = mem_malloc(sizeof(channel_tt));
    pHandle->pCodecStream = NULL;
    pHandle->pService     = NULL;
    pHandle->pEventIO     = pEventIO;
    atomic_init(&pHandle->hConnection, pEventConnection);
    atomic_init(&pHandle->hDisconnectTimeout, NULL);
    atomic_init(&pHandle->iRefCount, 1);
    pHandle->uiID = channelCenter_register(pHandle);
    atomic_init(&pHandle->iStatus, eStarting);
    return pHandle;
}

void channel_setCodecStream(channel_tt* pHandle, struct codecStream_s* pCodecStream)
{
    if (pHandle->pCodecStream) {
        if (pHandle->pCodecStream->fnRelease) {
            pHandle->pCodecStream->fnRelease(pHandle->pCodecStream);
        }
        pHandle->pCodecStream = NULL;
    }

    if (pCodecStream) {
        if (pCodecStream->fnAddref) {
            pCodecStream->fnAddref(pCodecStream);
        }
        pHandle->pCodecStream = pCodecStream;
    }
}

void channel_addref(channel_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void channel_release(channel_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->pService) {
            service_release(pHandle->pService);
            pHandle->pService = NULL;
        }

        if (pHandle->pCodecStream) {
            if (pHandle->pCodecStream->fnRelease) {
                pHandle->pCodecStream->fnRelease(pHandle->pCodecStream);
            }
            pHandle->pCodecStream = NULL;
        }
        mem_free(pHandle);
    }
}

bool channel_bind(channel_tt* pHandle, service_tt* pService, bool bKeepAlive, bool bTcpNoDelay)
{
    eventConnection_tt* pEventConnection = (eventConnection_tt*)atomic_load(&pHandle->hConnection);
    if (pEventConnection) {
        pHandle->pService = pService;
        service_addref(pHandle->pService);
        eventConnection_setReceiveCallback(pEventConnection, eventConnection_onReceiveCallback);
        eventConnection_setDisconnectCallback(pEventConnection,
                                              eventConnection_onDisconnectCallback);
        eventConnection_setCloseCallback(pEventConnection, eventConnection_onClose);
        atomic_fetch_add(&(pHandle->iRefCount), 1);
        atomic_store(&pHandle->iStatus, eRunning);
        if (!eventConnection_bind(
                pEventConnection, bKeepAlive, bTcpNoDelay, pHandle, eventConnection_onUserFree)) {
            atomic_store(&pHandle->iStatus, eStopped);
            eventConnection_tt* pConnectionHandle =
                (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
            eventConnection_release(pConnectionHandle);
            return false;
        }
        return true;
    }
    return false;
}

void channel_close(channel_tt* pHandle, int32_t iDisconnectTimeoutMs)
{
    if (iDisconnectTimeoutMs <= 0) {
        int32_t iStatus = atomic_exchange(&pHandle->iStatus, eStopped);
        if (iStatus != eStopped) {
            eventTimer_tt* pDisconnectTimeoutHandle =
                (eventTimer_tt*)atomic_exchange(&pHandle->hDisconnectTimeout, 0);
            if (pDisconnectTimeoutHandle) {
                eventTimer_stop(pDisconnectTimeoutHandle);
                eventTimer_release(pDisconnectTimeoutHandle);
            }

            eventConnection_tt* pConnectionHandle =
                (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
            if (pConnectionHandle) {
                eventConnection_forceClose(pConnectionHandle);
                eventConnection_release(pConnectionHandle);
            }
        }
    }
    else {
        int32_t iStatus = atomic_exchange(&pHandle->iStatus, eStopping);
        if (iStatus < eStopping) {
            eventTimer_tt* pDisconnectTimeoutHandle =
                createEventTimer(pHandle->pEventIO,
                                 eventTimer_onDisconnectTimeout,
                                 true,
                                 iDisconnectTimeoutMs,
                                 pHandle);
            atomic_store(&pHandle->hDisconnectTimeout, pDisconnectTimeoutHandle);
            eventTimer_setCloseCallback(pDisconnectTimeoutHandle, eventTimer_onClose);
            atomic_fetch_add(&(pHandle->iRefCount), 1);
            eventTimer_start(pDisconnectTimeoutHandle);
            eventConnection_tt* pEventConnection =
                (eventConnection_tt*)atomic_load(&pHandle->hConnection);
            if (pEventConnection) {
                eventConnection_close(pEventConnection);
            }
        }
    }
}

bool channel_getRemoteAddr(channel_tt* pHandle, inetAddress_tt* pOutInetAddress)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            eventConnection_getRemoteAddr(pEventConnection, pOutInetAddress);
            return true;
        }
    }
    return false;
}

bool channel_getLocalAddr(channel_tt* pHandle, inetAddress_tt* pOutInetAddress)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            eventConnection_getLocalAddr(pEventConnection, pOutInetAddress);
            return true;
        }
    }
    return false;
}

bool channel_isRunning(channel_tt* pHandle)
{
    return atomic_load(&pHandle->iStatus) == eRunning;
}

service_tt* channel_getService(channel_tt* pHandle)
{
    return pHandle->pService;
}

int32_t channel_sendMove(channel_tt* pHandle, ioBufVec_tt* pInBufVec, int32_t iCount,
                         uint32_t uiFlag, uint32_t uiToken)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            if (pHandle->pCodecStream && pHandle->pCodecStream->fnWriteMove) {
                return pHandle->pCodecStream->fnWriteMove(
                    pHandle->pCodecStream, pEventConnection, pInBufVec, iCount, uiFlag, uiToken);
            }
            return eventConnection_send(pEventConnection,
                                        createEventBuf_move(pInBufVec, iCount, NULL, 0));
        }
    }

    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return -1;
}

int32_t channel_send(channel_tt* pHandle, const char* pBuffer, int32_t iLength, uint32_t uiFlag,
                     uint32_t uiToken)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            if (pHandle->pCodecStream && pHandle->pCodecStream->fnWrite) {
                return pHandle->pCodecStream->fnWrite(
                    pHandle->pCodecStream, pEventConnection, pBuffer, iLength, uiFlag, uiToken);
            }
            return eventConnection_send(pEventConnection,
                                        createEventBuf(pBuffer, iLength, NULL, 0));
        }
    }
    return -1;
}

int32_t channel_writeMove(channel_tt* pHandle, ioBufVec_tt* pInBufVec, int32_t iCount,
                          uint32_t uiToken)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            if (uiToken != 0) {
                return eventConnection_send(
                    pEventConnection,
                    createEventBuf_move(
                        pInBufVec, iCount, eventConnection_onSendCompleteCallback, uiToken));
            }
            return eventConnection_send(pEventConnection,
                                        createEventBuf_move(pInBufVec, iCount, NULL, 0));
        }
    }

    for (int32_t i = 0; i < iCount; ++i) {
        mem_free(pInBufVec[i].pBuf);
        pInBufVec[i].pBuf    = NULL;
        pInBufVec[i].iLength = 0;
    }
    return -1;
}

int32_t channel_write(channel_tt* pHandle, const char* pBuffer, int32_t iLength, uint32_t uiToken)
{
    if (atomic_load(&pHandle->iStatus) == eRunning) {
        eventConnection_tt* pEventConnection =
            (eventConnection_tt*)atomic_load(&pHandle->hConnection);
        if (pEventConnection) {
            if (uiToken != 0) {
                return eventConnection_send(
                    pEventConnection,
                    createEventBuf(
                        pBuffer, iLength, eventConnection_onSendCompleteCallback, uiToken));
            }
            return eventConnection_send(pEventConnection,
                                        createEventBuf(pBuffer, iLength, NULL, 0));
        }
    }
    return -1;
}

bool channel_pushService(channel_tt* pHandle, byteQueue_tt* pByteQueue, uint32_t uiLength,
                         uint32_t uiFlag, uint32_t uiToken)
{
    serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + uiLength);
    pEvent->uiSourceID      = pHandle->uiID;
    pEvent->uiToken         = uiToken;
    if (uiLength > 0) {
        byteQueue_readBytes(pByteQueue, pEvent->szStorage, uiLength, false);
    }
    pEvent->uiLength = uiLength | (uiFlag << 24);
    if (!service_enqueue(pHandle->pService, pEvent)) {
        return false;
    }
    return true;
}

uint32_t channel_getID(channel_tt* pHandle)
{
    return pHandle->uiID;
}

int32_t channel_getWritePending(channel_tt* pHandle)
{
    eventConnection_tt* pEventConnection = (eventConnection_tt*)atomic_load(&pHandle->hConnection);
    if (pEventConnection) {
        return eventConnection_getWritePending(pEventConnection);
    }
    return 0;
}

size_t channel_getWritePendingBytes(channel_tt* pHandle)
{
    eventConnection_tt* pEventConnection = (eventConnection_tt*)atomic_load(&pHandle->hConnection);
    if (pEventConnection) {
        return eventConnection_getWritePendingBytes(pEventConnection);
    }
    return 0;
}

size_t channel_getReceiveBufLength(channel_tt* pHandle)
{
    eventConnection_tt* pEventConnection = (eventConnection_tt*)atomic_load(&pHandle->hConnection);
    if (pEventConnection) {
        return eventConnection_getReceiveBufLength(pEventConnection);
    }
    return 0;
}
