#include "eventIO/internal/posix/eventIOLoop_t.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "utility_t.h"
#include "time_t.h"

#include "eventIO/eventIO_t.h"

typedef struct eventIOLoopAsync_s
{
    eventIOLoop_tt* pEventIOLoop;
    eventAsync_tt   eventAsync;
} eventIOLoopAsync_tt;

static void inLoop_eventIOLoop_stop(eventAsync_tt* pEventAsync)
{
    eventIOLoopAsync_tt* pEventIOLoopAsync =
        container_of(pEventAsync, eventIOLoopAsync_tt, eventAsync);
    eventIOLoop_tt* pEventIOLoop = pEventIOLoopAsync->pEventIOLoop;
    if (pEventIOLoop->bRunning) {
        pEventIOLoop->bRunning = false;
    }
    mem_free(pEventIOLoopAsync);
}

static void eventIOLoop_doEvents(eventIOLoop_tt* pEventIOLoop)
{
    QUEUE queuePending;
#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIOLoop->spinLock);
#else
    mutex_lock(&pEventIOLoop->mutex);
#endif
    QUEUE_MOVE(&pEventIOLoop->queuePending, &queuePending);
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIOLoop->spinLock);
#else
    mutex_unlock(&pEventIOLoop->mutex);
#endif

    eventAsync_tt* pEvent = NULL;
    QUEUE*         pNode  = NULL;
    while (!QUEUE_EMPTY(&queuePending)) {
        pNode = QUEUE_HEAD(&queuePending);
        QUEUE_REMOVE(pNode);

        pEvent = container_of(pNode, eventAsync_tt, node);
        if (eventIO_isRunning(pEventIOLoop->pEventIO)) {
            pEvent->fnWork(pEvent);
        }
        else {
            if (pEvent->fnCancel) {
                pEvent->fnCancel(pEvent);
            }
        }
    }
}

static void eventIOLoop_queuedEvent(struct pollHandle_s* pHandle, int32_t iEvents)
{
    eventIOLoop_tt* pEventIOLoop = container_of(pHandle, eventIOLoop_tt, queuedHandle);
    if (iEvents & ePollerReadable) {
        char szBuf[128];
        if (read(pEventIOLoop->hQueuedEvent, szBuf, sizeof(szBuf)) > 0) {
            pEventIOLoop->fnDoEvents(pEventIOLoop->pEventIO);
        }
    }
}

void eventIOLoop_init(eventIOLoop_tt* pEventIOLoop, struct eventIO_s* pEventIO,
                      void (*fnStop)(struct eventIO_s*))
{
    pEventIOLoop->pPoller      = NULL;
    pEventIOLoop->fnStop       = fnStop;
    pEventIOLoop->fnDoEvents   = NULL;
    pEventIOLoop->bRunning     = false;
    pEventIOLoop->hQueuedEvent = -1;
    pEventIOLoop->pEventIO     = pEventIO;
    eventIO_addref(pEventIOLoop->pEventIO);
    atomic_init(&pEventIOLoop->iConnections, 0);
    wakeupEvent_init(&pEventIOLoop->wakeupEvent);
    pollHandle_init(&pEventIOLoop->queuedHandle);
    QUEUE_INIT(&pEventIOLoop->queuePending);
#ifdef DEF_USE_SPINLOCK
    spinLock_init(&pEventIOLoop->spinLock);
#else
    mutex_init(&pEventIOLoop->mutex);
#endif
}

void eventIOLoop_clear(eventIOLoop_tt* pEventIOLoop)
{
    eventAsync_tt* pEvent = NULL;
    QUEUE*         pNode  = NULL;
    while (!QUEUE_EMPTY(&pEventIOLoop->queuePending)) {
        pNode = QUEUE_HEAD(&pEventIOLoop->queuePending);
        QUEUE_REMOVE(pNode);

        pEvent = container_of(pNode, eventAsync_tt, node);
        if (pEvent->fnCancel) {
            pEvent->fnCancel(pEvent);
        }
    }

    if (pEventIOLoop->pPoller) {
        if (pEventIOLoop->hQueuedEvent != -1) {
            poller_clear(pEventIOLoop->pPoller,
                         pEventIOLoop->hQueuedEvent,
                         &pEventIOLoop->queuedHandle,
                         ePollerClosed);
            pEventIOLoop->hQueuedEvent = -1;
        }
        wakeupEvent_clear(&pEventIOLoop->wakeupEvent, pEventIOLoop->pPoller);
        poller_release(pEventIOLoop->pPoller);
        pEventIOLoop->pPoller = NULL;
    }
    else {
        wakeupEvent_clear(&pEventIOLoop->wakeupEvent, NULL);
    }

#ifndef DEF_USE_SPINLOCK
    mutex_destroy(&pEventIOLoop->mutex);
#endif
    if (pEventIOLoop->fnStop) {
        pEventIOLoop->fnStop(pEventIOLoop->pEventIO);
    }
    eventIO_release(pEventIOLoop->pEventIO);
}

bool eventIOLoop_start(eventIOLoop_tt* pEventIOLoop, int32_t hQueuedEvent,
                       void (*fnDoEvents)(struct eventIO_s*))
{
    pEventIOLoop->uiThreadId = threadId();
    pEventIOLoop->fnDoEvents = fnDoEvents;
    if (pEventIOLoop->pPoller == NULL) {
        pEventIOLoop->pPoller = createPoller();
        if (!wakeupEvent_start(&pEventIOLoop->wakeupEvent,
                               pEventIOLoop,
                               pEventIOLoop->pPoller,
                               eventIOLoop_doEvents)) {
            poller_release(pEventIOLoop->pPoller);
            pEventIOLoop->pPoller = NULL;
            return false;
        }
    }
    if (hQueuedEvent != -1) {
        pEventIOLoop->hQueuedEvent = hQueuedEvent;
        poller_add(pEventIOLoop->pPoller,
                   pEventIOLoop->hQueuedEvent,
                   &pEventIOLoop->queuedHandle,
                   ePollerReadable,
                   eventIOLoop_queuedEvent);
    }
    pEventIOLoop->bRunning = true;
    return true;
}

void eventIOLoop_stop(eventIOLoop_tt* pEventIOLoop)
{
    eventIOLoopAsync_tt* pEventIOAsync = mem_malloc(sizeof(eventIOLoopAsync_tt));
    pEventIOAsync->pEventIOLoop        = pEventIOLoop;
    eventIOLoop_queueInLoop(
        pEventIOLoop, &pEventIOAsync->eventAsync, inLoop_eventIOLoop_stop, inLoop_eventIOLoop_stop);
}

void eventIOLoop_threadRun(void* pArg)
{
    eventIOLoop_tt* pEventIOLoop = (eventIOLoop_tt*)pArg;
    pEventIOLoop->uiThreadId     = threadId();
    eventIO_tt* pEventIO         = pEventIOLoop->pEventIO;
    int32_t     iEvents          = 0;
    while (pEventIOLoop->bRunning) {
        atomic_fetch_add(&pEventIO->iIdleThreads, 1);
        iEvents = poller_wait(pEventIOLoop->pPoller, -1);
        atomic_fetch_sub(&pEventIO->iIdleThreads, 1);
        if (iEvents == -1) {
            break;
        }
        else if (iEvents > 0) {
            poller_dispatch(pEventIOLoop->pPoller, iEvents);
        }
    }
    eventIOLoop_clear(pEventIOLoop);
}
