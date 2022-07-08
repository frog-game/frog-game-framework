

#include "internal/timerWatcher_t.h"

#include "internal/service-inl.h"
#include "serviceEvent_t.h"
#include "service_t.h"

static void eventTimer_onTriggered(eventTimer_tt* pEventTimer, void* pData)
{
    timerWatcher_tt* pTimerWatcher = (timerWatcher_tt*)pData;

    serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + sizeof(timerWatcher_tt*));
    pEvent->uiSourceID      = service_getID(pTimerWatcher->pService);
    pEvent->uiToken         = pTimerWatcher->uiToken;
    *(timerWatcher_tt**)(pEvent->szStorage) = pTimerWatcher;
    if (!eventTimer_isOnce(pEventTimer)) {
        pEvent->uiLength = DEF_EVENT_RUN_EVERY << 24;
    }
    else {
        pEvent->uiLength = DEF_EVENT_RUN_AFTER << 24;
    }
    timerWatcher_addref(pTimerWatcher);
    if (!service_enqueue(pTimerWatcher->pService, pEvent)) {
        timerWatcher_release(pTimerWatcher);
    }
}

void timerWatcher_eventTimer_onClose(eventTimer_tt* pHandle, void* pData)
{
    timerWatcher_tt* pTimerWatcher = (timerWatcher_tt*)pData;
    timerWatcher_release(pTimerWatcher);
}

timerWatcher_tt* createTimerWatcher(service_tt* pService, uint32_t uiToken)
{
    timerWatcher_tt* pHandle = mem_malloc(sizeof(timerWatcher_tt));
    pHandle->pService        = pService;
    service_addref(pHandle->pService);
    pHandle->uiToken     = uiToken;
    pHandle->pEventTimer = NULL;
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void timerWatcher_addref(timerWatcher_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void timerWatcher_release(timerWatcher_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->pEventTimer) {
            eventTimer_release(pHandle->pEventTimer);
            pHandle->pEventTimer = NULL;
        }

        if (pHandle->pService) {
            service_release(pHandle->pService);
            pHandle->pService = NULL;
        }
        mem_free(pHandle);
    }
}

bool timerWatcher_start(timerWatcher_tt* pHandle, bool bOnce, uint32_t uiIntervalMs)
{
    eventIO_tt* pEventIO = service_getEventIO(pHandle->pService);
    atomic_fetch_add(&(pHandle->iRefCount), 1);

    pHandle->pEventTimer =
        createEventTimer(pEventIO, eventTimer_onTriggered, bOnce, uiIntervalMs, pHandle);
    eventTimer_setCloseCallback(pHandle->pEventTimer, timerWatcher_eventTimer_onClose);
    return eventTimer_start(pHandle->pEventTimer);
}

void timerWatcher_stop(timerWatcher_tt* pHandle)
{
    if (pHandle->pEventTimer) {
        eventTimer_stop(pHandle->pEventTimer);
        eventTimer_release(pHandle->pEventTimer);
        pHandle->pEventTimer = NULL;
    }
}

bool timerWatcher_isRunning(timerWatcher_tt* pHandle)
{
    return pHandle->pEventTimer;
}

service_tt* timerWatcher_getService(timerWatcher_tt* pHandle)
{
    return pHandle->pService;
}
