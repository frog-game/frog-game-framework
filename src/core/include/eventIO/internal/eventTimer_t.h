#pragma once

#include <stdatomic.h>
#include <stdlib.h>

#include "heap_t.h"
#include "utility_t.h"
#include "eventIO/eventAsync_t.h"

#if defined(_WINDOWS) || defined(_WIN32)
#    include "eventIO/internal/win/eventIO-inl.h"
#else
#    include "eventIO/internal/posix/eventIO-inl.h"
#endif

#include "eventIO/eventIO_t.h"

typedef struct eventTimerAsync_s
{
    struct eventTimer_s* pEventTimer;
    eventAsync_tt        eventAsync;
} eventTimerAsync_tt;

struct eventTimer_s
{
    void (*fn)(struct eventTimer_s*, void*);
    void (*fnCloseCallback)(struct eventTimer_s*, void*);
    void*             pUserData;
    struct eventIO_s* pEventIO;
    bool              bOnce;
    bool              bActive;
    atomic_bool       bRunning;
    struct heap_node  node;
    uint64_t          uiTimeout;
    uint64_t          uiID;
    uint32_t          uiIntervalMs;
    atomic_int        iRefCount;
};

static inline int32_t timerLessThan(const struct heap_node* pFirst, const struct heap_node* pSecond)
{
    const struct eventTimer_s* pTimerFirst  = container_of(pFirst, struct eventTimer_s, node);
    const struct eventTimer_s* pTimerSecond = container_of(pSecond, struct eventTimer_s, node);

    if (pTimerFirst->uiTimeout < pTimerSecond->uiTimeout) {
        return 1;
    }

    if (pTimerFirst->uiTimeout > pTimerSecond->uiTimeout) {
        return 0;
    }

    if (pTimerFirst->uiID < pTimerSecond->uiID) {
        return 1;
    }

    if (pTimerFirst->uiID > pTimerSecond->uiID) {
        return 0;
    }

    return 0;
}

static inline void eventTimer_run(struct eventTimer_s* pHandle)
{
    heap_remove(&pHandle->pEventIO->timerHeap, &pHandle->node, timerLessThan);
    if (pHandle->bOnce) {
        pHandle->bActive = false;

        bool bRunning = true;
        if (atomic_compare_exchange_strong(&pHandle->bRunning, &bRunning, false)) {
            pHandle->fn(pHandle, pHandle->pUserData);
        }

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
        }
        eventTimer_release(pHandle);
    }
    else {
        if (atomic_load(&pHandle->bRunning)) {
            pHandle->uiTimeout = pHandle->pEventIO->uiLoopTime + pHandle->uiIntervalMs;
            pHandle->uiID      = pHandle->pEventIO->uiTimerCounter++;
            heap_insert(&pHandle->pEventIO->timerHeap, &pHandle->node, timerLessThan);
            pHandle->fn(pHandle, pHandle->pUserData);
        }
        else {
            pHandle->bActive = false;
            if (pHandle->fnCloseCallback) {
                pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
            }
            eventTimer_release(pHandle);
        }
    }
}

static inline void inLoop_eventTimer_cancel(eventAsync_tt* pEventAsync)
{
    eventTimerAsync_tt* pEventTimerAsync =
        container_of(pEventAsync, eventTimerAsync_tt, eventAsync);
    struct eventTimer_s* pHandle = pEventTimerAsync->pEventTimer;
    eventTimer_release(pHandle);
    mem_free(pEventTimerAsync);
}

static inline void inLoop_eventTimer_start(eventAsync_tt* pEventAsync)
{
    eventTimerAsync_tt* pEventTimerAsync =
        container_of(pEventAsync, eventTimerAsync_tt, eventAsync);
    struct eventTimer_s* pHandle = pEventTimerAsync->pEventTimer;
    if (pHandle->uiIntervalMs == 0) {
        bool bRunning = true;
        if (atomic_compare_exchange_strong(&pHandle->bRunning, &bRunning, false)) {
            pHandle->fn(pHandle, pHandle->pUserData);
        }

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
        }
        eventTimer_release(pHandle);
    }
    else {
        if (atomic_load(&pHandle->bRunning)) {
            pHandle->uiTimeout = pHandle->pEventIO->uiLoopTime + pHandle->uiIntervalMs;
            pHandle->uiID      = pHandle->pEventIO->uiTimerCounter++;

            heap_insert(&pHandle->pEventIO->timerHeap, &pHandle->node, timerLessThan);
            pHandle->bActive = true;
        }
        else {
            if (pHandle->fnCloseCallback) {
                pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
            }
            eventTimer_release(pHandle);
        }
    }
    mem_free(pEventTimerAsync);
}

static inline void inLoop_eventTimer_stop(eventAsync_tt* pEventAsync)
{
    eventTimerAsync_tt* pEventTimerAsync =
        container_of(pEventAsync, eventTimerAsync_tt, eventAsync);
    struct eventTimer_s* pHandle = pEventTimerAsync->pEventTimer;

    if (pHandle->bActive) {
        pHandle->bActive = false;
        heap_remove(&pHandle->pEventIO->timerHeap, &pHandle->node, timerLessThan);

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
        }
        eventTimer_release(pHandle);
    }

    eventTimer_release(pHandle);
    mem_free(pEventTimerAsync);
}