

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "queue_t.h"
#include "spinLock_t.h"
#include "thread_t.h"
#include "utility_t.h"

#include "eventIO/eventIO_t.h"

typedef struct serviceEvent_s
{
    void*    node[2];
    uint32_t uiSourceID;
    uint32_t uiToken;
    uint32_t uiLength;
    char     szStorage[];
} serviceEvent_tt;

#define DEF_USE_SPINLOCK

struct service_s
{
    void (*fnStop)(void*);
    bool (*fnCallback)(int32_t, uint32_t, uint32_t, void*, size_t, void*);
    void*            pUserData;
    eventIO_tt*      pEventIO;
    eventWatcher_tt* pEventWatcher;
    uint32_t         uiServiceID;
    QUEUE            queuePending;
#ifdef DEF_USE_SPINLOCK
    spinLock_tt spinLock;
#else
    mutex_tt mutex;
#endif
    atomic_bool bRunning;
    atomic_int  iRefCount;
    atomic_uint uiQueueSize;
};

__UNUSED void service_waitFor();

__UNUSED void service_wakeUp();

__UNUSED int32_t service_waitForCount();

static inline void service_notify(struct service_s* pService)
{
    if (eventWatcher_notify(pService->pEventWatcher)) {
        service_wakeUp();
    }
}

static inline bool service_enqueue(struct service_s* pService, serviceEvent_tt* pEvent)
{
    if (atomic_load(&pService->bRunning)) {
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
        return true;
    }
    mem_free(pEvent);
    return false;
}