#include "eventIO/internal/eventTimer_t.h"
#include "eventIO/eventIO_t.h"

eventTimer_tt* createEventTimer(eventIO_tt* pEventIO, void (*fn)(eventTimer_tt*, void*), bool bOnce,
                                uint32_t uiIntervalMs, void* pUserData)
{
    eventTimer_tt* pHandle   = (eventTimer_tt*)mem_malloc(sizeof(eventTimer_tt));
    pHandle->pEventIO        = pEventIO;
    pHandle->bOnce           = bOnce;
    pHandle->pUserData       = pUserData;
    pHandle->uiID            = 0xffffffffffffffff;
    pHandle->uiIntervalMs    = uiIntervalMs;
    pHandle->uiTimeout       = 0;
    pHandle->fn              = fn;
    pHandle->fnCloseCallback = NULL;
    pHandle->bActive         = false;
    atomic_init(&pHandle->bRunning, false);
    atomic_init(&pHandle->iRefCount, 1);

    return pHandle;
}

void eventTimer_addref(eventTimer_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void eventTimer_release(eventTimer_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        mem_free(pHandle);
    }
}

void eventTimer_setCloseCallback(eventTimer_tt* pHandle, void (*fn)(eventTimer_tt*, void*))
{
    pHandle->fnCloseCallback = fn;
}

bool eventTimer_start(eventTimer_tt* pHandle)
{
    bool bRunning = false;
    if (atomic_compare_exchange_strong(&pHandle->bRunning, &bRunning, true)) {
        atomic_fetch_add(&pHandle->iRefCount, 1);
        eventTimerAsync_tt* pEventTimerAsync = mem_malloc(sizeof(eventTimerAsync_tt));
        pEventTimerAsync->pEventTimer        = pHandle;
        eventIO_runInLoop(pHandle->pEventIO,
                          &pEventTimerAsync->eventAsync,
                          inLoop_eventTimer_start,
                          inLoop_eventTimer_cancel);
        return true;
    }
    return false;
}

void eventTimer_stop(eventTimer_tt* pHandle)
{
    bool bRunning = true;
    if (atomic_compare_exchange_strong(&pHandle->bRunning, &bRunning, false)) {
        eventTimerAsync_tt* pEventTimerAsync = mem_malloc(sizeof(eventTimerAsync_tt));
        pEventTimerAsync->pEventTimer        = pHandle;
        atomic_fetch_add(&(pHandle->iRefCount), 1);
        eventIO_runInLoop(pHandle->pEventIO,
                          &pEventTimerAsync->eventAsync,
                          inLoop_eventTimer_stop,
                          inLoop_eventTimer_stop);
    }
}

bool eventTimer_isRunning(eventTimer_tt* pHandle)
{
    return atomic_load(&pHandle->bRunning);
}

bool eventTimer_isOnce(eventTimer_tt* pHandle)
{
    return pHandle->bOnce;
}
