#pragma once

#include <stdatomic.h>

#include <unistd.h>
#include <sys/socket.h>

#include "queue_t.h"
#include "heap_t.h"
#include "thread_t.h"
#include "spinLock_t.h"

#include "eventIO/internal/posix/poller_t.h"
#include "eventIO/eventAsync_t.h"

#define DEF_USE_SPINLOCK

struct eventIO_s
{
    struct eventIOLoop_s* pEventIOLoop;
    uint32_t              uiCocurrentThreads;
    uint64_t              uiLoopTime;
    uint64_t              uiTimerCounter;
    struct heap           timerHeap;
    bool                  bTimerEventOff;
    bool                  bRunning;
    atomic_uint           uiCocurrentRunning;
    atomic_int            iIdleThreads;
    int32_t               hQueuedEvent[2];
    QUEUE                 queuedEvent;
#ifdef DEF_USE_SPINLOCK
    spinLock_tt queuedLock;
#else
    mutex_tt queuedLock;
#endif
    QUEUE       queuePending;
    mutex_tt    mutex;
    uint64_t    uiThreadId;
    cond_tt     cond;
    atomic_bool bLoopRunning;
    atomic_int  iRefCount;
};

static inline bool eventIO_isRunning(struct eventIO_s* pEventIO)
{
    return atomic_load(&pEventIO->bLoopRunning);
}

static inline void eventIO_postQueued(struct eventIO_s* pEventIO, eventAsync_tt* pEventAsync,
                                      void (*fnWork)(eventAsync_tt*),
                                      void (*fnCancel)(eventAsync_tt*))
{
    if (atomic_load(&pEventIO->bLoopRunning)) {
        pEventAsync->fnWork   = fnWork;
        pEventAsync->fnCancel = fnCancel;

#ifdef DEF_USE_SPINLOCK
        spinLock_lock(&pEventIO->queuedLock);
#else
        mutex_lock(&pEventIO->queuedLock);
#endif
        QUEUE_INSERT_TAIL(&pEventIO->queuedEvent, &pEventAsync->node);
#ifdef DEF_USE_SPINLOCK
        spinLock_unlock(&pEventIO->queuedLock);
#else
        mutex_unlock(&pEventIO->queuedLock);
#endif
        uint64_t once = 1;
#if DEF_PLATFORM == DEF_PLATFORM_LINUX
        write(pEventIO->hQueuedEvent[0], &once, sizeof once);
#else
        write(pEventIO->hQueuedEvent[1], &once, sizeof once);
#endif
    }
    else {
        if (fnCancel) {
            fnCancel(pEventAsync);
        }
    }
}

__UNUSED struct eventIOLoop_s* eventIO_connectionLoop(struct eventIO_s* pEventIO);