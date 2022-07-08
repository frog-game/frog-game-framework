

#include "service_t.h"

#include "eventIO/eventIO_t.h"

#include "internal/service-inl.h"

#include "inetAddress_t.h"

#include "channel/channelCenter_t.h"
#include "channel/channel_t.h"
#include "serviceCenter_t.h"
#include "serviceMonitor_t.h"

#include "internal/connector_t.h"
#include "internal/dnsResolve_t.h"
#include "internal/listenPort_t.h"
#include "internal/timerWatcher_t.h"
#include "serviceEvent_t.h"

static atomic_int s_iWaitforService = ATOMIC_VAR_INIT(0);

void service_waitFor()
{
    atomic_fetch_add(&s_iWaitforService, 1);
}

void service_wakeUp()
{
    atomic_fetch_sub(&s_iWaitforService, 1);
}

int32_t service_waitForCount()
{
    return atomic_load(&s_iWaitforService);
}

static inline bool service_eventCallback(service_tt* pService, serviceEvent_tt* pEvent)
{
    int32_t iEvent   = pEvent->uiLength >> 24;
    bool    bMoveBuf = iEvent & DEF_EVENT_MOVEBUF;
    int32_t iType    = iEvent & DEF_EVENT_MASK;
    switch (iType) {
    case DEF_EVENT_MSG:
    {
        int32_t iEventMsg = iEvent & DEF_EVENT_MSG_MASK;
        switch (iEventMsg) {
        case DEF_EVENT_MSG_REPLY:
        {
            size_t nLength = pEvent->uiLength & 0xFFFFFF;
            if (bMoveBuf) {
                void* pBuffer = *(void**)pEvent->szStorage;
                pService->fnCallback(iEventMsg | DEF_EVENT_MSG,
                                     pEvent->uiSourceID,
                                     pEvent->uiToken,
                                     pBuffer,
                                     nLength,
                                     pService->pUserData);
                mem_free(pBuffer);
            }
            else {
                pService->fnCallback(iEventMsg | DEF_EVENT_MSG,
                                     pEvent->uiSourceID,
                                     pEvent->uiToken,
                                     pEvent->szStorage,
                                     nLength,
                                     pService->pUserData);
            }
        } break;
        case DEF_EVENT_MSG_CALL:
        case DEF_EVENT_MSG_SEND:
        case DEF_EVENT_MSG_TEXT:
        {
            size_t nLength = pEvent->uiLength & 0xFFFFFF;
            if (bMoveBuf) {
                void* pBuffer = *(void**)pEvent->szStorage;
                pService->fnCallback(iEventMsg | DEF_EVENT_MSG,
                                     pEvent->uiSourceID,
                                     pEvent->uiToken,
                                     pBuffer,
                                     nLength,
                                     pService->pUserData);
                mem_free(pBuffer);
            }
            else {
                pService->fnCallback(iEventMsg | DEF_EVENT_MSG,
                                     pEvent->uiSourceID,
                                     pEvent->uiToken,
                                     pEvent->szStorage,
                                     nLength,
                                     pService->pUserData);
            }
        } break;
        case DEF_EVENT_MSG_PING:
        case DEF_EVENT_MSG_PONG:
        case DEF_EVENT_MSG_CLOSE:
        {
            pService->fnCallback(iEventMsg | DEF_EVENT_MSG,
                                 pEvent->uiSourceID,
                                 pEvent->uiToken,
                                 NULL,
                                 0,
                                 pService->pUserData);
        } break;
        }
    } break;
    case DEF_EVENT_RUN_AFTER:
    case DEF_EVENT_RUN_EVERY:
    {
        timerWatcher_tt* pTimerWatcher = *(timerWatcher_tt**)(pEvent->szStorage);
        if (timerWatcher_isRunning(pTimerWatcher)) {
            pService->fnCallback(
                iType, pEvent->uiSourceID, pEvent->uiToken, NULL, 0, pService->pUserData);
        }
        timerWatcher_release(pTimerWatcher);
    } break;
    case DEF_EVENT_YIELD:
    {
        pService->fnCallback(
            iType, pEvent->uiSourceID, pEvent->uiToken, NULL, 0, pService->pUserData);
    } break;
    case DEF_EVENT_ACCEPT:
    {
        size_t              nLength          = pEvent->uiLength & 0xFFFFFF;
        eventConnection_tt* pEventConnection = *(eventConnection_tt**)(pEvent->szStorage);
        if (pEventConnection) {
            channel_tt* pChannel    = createChannel(pService->pEventIO, pEventConnection);
            uint32_t    uiChannelID = channel_getID(pChannel);
            void*       pBuffer     = NULL;
            if (nLength) {
                pBuffer = pEvent->szStorage + sizeof(eventConnection_tt*);
            }
            if (!pService->fnCallback(
                    iType, uiChannelID, pEvent->uiToken, pBuffer, nLength, pService->pUserData)) {
                channel_close(pChannel, 0);
                channel_release(pChannel);
            }
        }
        else {
            pService->fnCallback(iType, 0, pEvent->uiToken, NULL, 0, pService->pUserData);
        }
    } break;
    case DEF_EVENT_CONNECT:
    {
        eventConnection_tt* pEventConnection = *(eventConnection_tt**)(pEvent->szStorage);
        if (pEventConnection != NULL) {
            if (!eventConnection_isConnecting(pEventConnection)) {
                eventConnection_release(pEventConnection);
                pService->fnCallback(iType, 0, pEvent->uiToken, NULL, 0, pService->pUserData);
            }
            else {
                channel_tt* pChannel    = createChannel(pService->pEventIO, pEventConnection);
                uint32_t    uiChannelID = channel_getID(pChannel);
                if (!pService->fnCallback(
                        iType, uiChannelID, pEvent->uiToken, NULL, 0, pService->pUserData)) {
                    channel_close(pChannel, 0);
                    channel_release(pChannel);
                }
            }
        }
        else {
            pService->fnCallback(iType, 0, pEvent->uiToken, NULL, 0, pService->pUserData);
        }
    } break;
    case DEF_EVENT_DNS:
    {
        size_t nLength = pEvent->uiLength & 0xFFFFFF;
        if (nLength == 0) {
            pService->fnCallback(
                iType, pEvent->uiSourceID, pEvent->uiToken, NULL, 0, pService->pUserData);
        }
        else {
            pService->fnCallback(iType,
                                 pEvent->uiSourceID,
                                 pEvent->uiToken,
                                 pEvent->szStorage,
                                 nLength,
                                 pService->pUserData);
        }
    } break;
    case DEF_EVENT_BINARY:
    {
        size_t nLength = pEvent->uiLength & 0xFFFFFF;
        if (bMoveBuf) {
            void* pBuffer = *(void**)pEvent->szStorage;
            pService->fnCallback(
                iType, pEvent->uiSourceID, pEvent->uiToken, pBuffer, nLength, pService->pUserData);
            mem_free(pBuffer);
        }
        else {
            pService->fnCallback(iType,
                                 pEvent->uiSourceID,
                                 pEvent->uiToken,
                                 pEvent->szStorage,
                                 nLength,
                                 pService->pUserData);
        }
    } break;
    case DEF_EVENT_SEND_OK:
    case DEF_EVENT_DISCONNECT:
    {
        pService->fnCallback(
            iType, pEvent->uiSourceID, pEvent->uiToken, NULL, 0, pService->pUserData);
    } break;
    case DEF_EVENT_COMMAND:
    {
        size_t nLength = pEvent->uiLength & 0xFFFFFF;
        if (bMoveBuf) {
            void* pBuffer = *(void**)pEvent->szStorage;
            pService->fnCallback(
                iType, pEvent->uiSourceID, pEvent->uiToken, pBuffer, nLength, pService->pUserData);
            mem_free(pBuffer);
        }
        else {
            pService->fnCallback(iType,
                                 pEvent->uiSourceID,
                                 pEvent->uiToken,
                                 pEvent->szStorage,
                                 nLength,
                                 pService->pUserData);
        }
    } break;
    case DEF_EVENT_SERVICE_STOP:
    {
        if (pService->pUserData && pService->fnStop) {
            pService->fnStop(pService->pUserData);
            pService->pUserData = NULL;
        }
        if (pService->uiServiceID != 0) {
            serviceCenter_deregister(pService->uiServiceID);
            pService->uiServiceID = 0;
        }

        if (pService->pEventWatcher) {
            eventWatcher_tt* pEventWatcher = pService->pEventWatcher;
            pService->pEventWatcher        = NULL;
            eventWatcher_reset(pEventWatcher);
            eventWatcher_close(pEventWatcher);
            eventWatcher_release(pEventWatcher);
        }
        return false;
    } break;
    }
    return true;
}

static void doPendingFunctors(eventWatcher_tt* pEventWatcher, void* pData)
{
    service_tt*      pService = (service_tt*)pData;
    serviceEvent_tt* pEvent   = NULL;
    QUEUE            queuePending;
    QUEUE*           pNode = NULL;

    int32_t iThreadIndex = 0;
    service_wakeUp();
    bool bRunning = true;

    for (;;) {
#ifdef DEF_USE_SPINLOCK
        spinLock_lock(&pService->spinLock);
#else
        mutex_lock(&pService->mutex);
#endif
        QUEUE_MOVE(&pService->queuePending, &queuePending);
#ifdef DEF_USE_SPINLOCK
        spinLock_unlock(&pService->spinLock);
#else
        mutex_unlock(&pService->mutex);
#endif
        if (QUEUE_EMPTY(&queuePending)) {
            if (pService->pEventWatcher) {
                eventWatcher_reset(pService->pEventWatcher);
            }

            if (atomic_load(&pService->uiQueueSize) > 0) {
                service_notify(pService);
            }
            return;
        }

        do {
            pNode  = QUEUE_HEAD(&queuePending);
            pEvent = container_of(pNode, serviceEvent_tt, node);
            QUEUE_REMOVE(pNode);
            atomic_fetch_sub(&pService->uiQueueSize, 1);
            iThreadIndex = serviceMonitor_enter(pEvent->uiSourceID, pService->uiServiceID);
            assert(bRunning);
            bRunning = service_eventCallback(pService, pEvent);
            mem_free(pEvent);
            serviceMonitor_leave(iThreadIndex);
        } while (!QUEUE_EMPTY(&queuePending));

        if (bRunning) {
            if ((atomic_load(&pService->uiQueueSize) > 0) && (service_waitForCount() > 0)) {
                if (pService->pEventWatcher) {
                    eventWatcher_reset(pService->pEventWatcher);
                }
                service_notify(pService);
                return;
            }
        }
        else {
            return;
        }
    };
}

static void eventWatcher_OnUserFree(void* pData)
{
    service_tt* pService = (service_tt*)pData;
    service_release(pService);
}

service_tt* createService(eventIO_tt* pEventIO)
{
    service_tt* pHandle  = mem_malloc(sizeof(service_tt));
    pHandle->pEventIO    = pEventIO;
    pHandle->pUserData   = NULL;
    pHandle->fnStop      = NULL;
    pHandle->fnCallback  = NULL;
    pHandle->uiServiceID = 0;
    atomic_init(&pHandle->iRefCount, 1);
    atomic_init(&pHandle->bRunning, false);
    atomic_init(&pHandle->uiQueueSize, 0);

#ifdef DEF_USE_SPINLOCK
    spinLock_init(&pHandle->spinLock);
#else
    mutex_init(&pHandle->mutex);
#endif
    QUEUE_INIT(&pHandle->queuePending);
    pHandle->pEventWatcher = NULL;
    return pHandle;
}

void service_setCallback(service_tt* pService,
                         bool (*fn)(int32_t, uint32_t, uint32_t, void*, size_t, void*))
{
    pService->fnCallback = fn;
}

void service_addref(service_tt* pService)
{
    atomic_fetch_add(&(pService->iRefCount), 1);
}

void service_release(service_tt* pService)
{
    if (atomic_fetch_sub(&(pService->iRefCount), 1) == 1) {
#ifndef DEF_USE_SPINLOCK
        mutex_destroy(&pService->mutex);
#endif
        mem_free(pService);
    }
}

uint32_t service_start(service_tt* pService, void* pUserData, bool (*fnStart)(void*),
                       void (*fnStop)(void*))
{
    bool bRunning = false;
    if (atomic_compare_exchange_strong(&pService->bRunning, &bRunning, true)) {
        atomic_fetch_add(&pService->iRefCount, 1);
        pService->pEventWatcher = createEventWatcher(
            pService->pEventIO, false, doPendingFunctors, pService, eventWatcher_OnUserFree);
        pService->uiServiceID = serviceCenter_register(pService);

        if (fnStart) {
            if (!fnStart(pUserData)) {
                atomic_store(&pService->bRunning, false);
                if (fnStop) {
                    fnStop(pUserData);
                }
                return 0;
            }
        }

        pService->pUserData = pUserData;
        pService->fnStop    = fnStop;
        eventWatcher_start(pService->pEventWatcher);
        service_notify(pService);
        return pService->uiServiceID;
    }
    return 0;
}

void service_stop(service_tt* pService)
{
    bool bRunning = true;
    if (atomic_compare_exchange_strong(&pService->bRunning, &bRunning, false)) {
        serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt));
        pEvent->uiLength        = DEF_EVENT_SERVICE_STOP << 24;
        pEvent->uiSourceID      = pService->uiServiceID;
        pEvent->uiToken         = 0;
        atomic_fetch_add(&pService->uiQueueSize, 1);
#ifdef DEF_USE_SPINLOCK
        spinLock_lock(&pService->spinLock);
#else
        mutex_lock(&pService->mutex);
#endif
        QUEUE_INSERT_TAIL(&pService->queuePending, &pEvent->node);
#ifdef DEF_USE_SPINLOCK
        spinLock_unlock(&pService->spinLock);
#else
        mutex_unlock(&pService->mutex);
#endif
        service_notify(pService);
    }
}

bool service_sendMove(service_tt* pService, uint32_t uiSourceID, void* pData, int32_t iLength,
                      uint32_t uiFlag, uint32_t uiToken)
{
    assert(iLength <= 0xFFFFFF);
    serviceEvent_tt* pEvent      = mem_malloc(sizeof(serviceEvent_tt) + sizeof(intptr_t));
    pEvent->uiLength             = iLength | (DEF_EVENT_MOVEBUF | uiFlag) << 24;
    pEvent->uiSourceID           = uiSourceID;
    pEvent->uiToken              = uiToken;
    *(void**)(pEvent->szStorage) = pData;
    if (!service_enqueue(pService, pEvent)) {
        mem_free(pData);
        return false;
    }
    return true;
}

bool service_send(service_tt* pService, uint32_t uiSourceID, const void* pData, int32_t iLength,
                  uint32_t uiFlag, uint32_t uiToken)
{
    assert(iLength <= 0xFFFFFF);
    serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + iLength);
    pEvent->uiLength        = iLength | uiFlag << 24;
    pEvent->uiSourceID      = uiSourceID;
    pEvent->uiToken         = uiToken;
    if (iLength != 0) {
        memcpy(pEvent->szStorage, pData, iLength);
    }
    return service_enqueue(pService, pEvent);
}

uint32_t service_queueSize(service_tt* pService)
{
    return atomic_load(&pService->uiQueueSize);
}

uint32_t service_getID(service_tt* pService)
{
    return pService->uiServiceID;
}


eventIO_tt* service_getEventIO(service_tt* pService)
{
    return pService->pEventIO;
}
