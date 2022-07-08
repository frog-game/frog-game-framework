

#include "eventIO/internal/win/eventWatcher_t.h"

#include <stdlib.h>

#include "utility_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/internal/win/iocpExt_t.h"
#include "eventIO/internal/win/eventIO-inl.h"

static inline void inLoop_eventWatcher_notify(eventWatcher_tt* pHandle)
{
    int32_t iStatus = 2;
    if (pHandle->bManualReset) {
        atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, 1);
    }
    else {
        atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, 3);
    }

    switch (iStatus) {
    case 0:
    {
        eventWatcher_release(pHandle);
    } break;
    case 2:
    {
        if (pHandle->fn) {
            pHandle->fn(pHandle, pHandle->pUserData);
        }
    } break;
    }
}

eventWatcher_tt* createEventWatcher(eventIO_tt* pEventIO, bool                 bManualReset,
                                    void (*fn)(eventWatcher_tt*, void*), void* pUserData,
                                    void (*fnUserFree)(void*))
{
    eventWatcher_tt* pHandle             = (eventWatcher_tt*)mem_malloc(sizeof(eventWatcher_tt));
    pHandle->pEventIO                    = pEventIO;
    pHandle->bManualReset                = bManualReset;
    pHandle->pUserData                   = pUserData;
    pHandle->fn                          = fn;
    pHandle->fnUserFree                  = fnUserFree;
    pHandle->notifiedEvent.pEventWatcher = pHandle;
    pHandle->notifiedEvent.fn            = inLoop_eventWatcher_notify;
    atomic_init(&pHandle->iStatus, 0);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void eventWatcher_addref(eventWatcher_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void eventWatcher_release(eventWatcher_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->fnUserFree) {
            pHandle->fnUserFree(pHandle->pUserData);
        }
        mem_free(pHandle);
    }
}

bool eventWatcher_start(eventWatcher_tt* pHandle)
{
    int32_t iStatus = 0;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, 1)) {
        atomic_fetch_add(&pHandle->iRefCount, 1);
        return true;
    }
    return false;
}

bool eventWatcher_isRunning(eventWatcher_tt* pHandle)
{
    return atomic_load(&pHandle->iStatus) != 0;
}

void eventWatcher_close(eventWatcher_tt* pHandle)
{
    int32_t iStatus = atomic_exchange(&pHandle->iStatus, 0);
    if (iStatus == 1) {
        bzero(&(pHandle->notifiedEvent._Overlapped), sizeof(OVERLAPPED));
        PostQueuedCompletionStatus(pHandle->pEventIO->hCompletionPort,
                                   0,
                                   def_IOCP_EVENT,
                                   &(pHandle->notifiedEvent._Overlapped));
    }
}

bool eventWatcher_notify(eventWatcher_tt* pHandle)
{
    int32_t iStatus = 1;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, 2)) {
        bzero(&(pHandle->notifiedEvent._Overlapped), sizeof(OVERLAPPED));
        PostQueuedCompletionStatus(pHandle->pEventIO->hCompletionPort,
                                   0,
                                   def_IOCP_EVENT,
                                   &(pHandle->notifiedEvent._Overlapped));
        return true;
    }
    return false;
}

void eventWatcher_reset(eventWatcher_tt* pHandle)
{
    int32_t iStatus = 3;
    atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, 1);
}
