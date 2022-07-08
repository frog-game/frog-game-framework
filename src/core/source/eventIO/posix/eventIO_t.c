#include "eventIO/eventIO_t.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

#if DEF_PLATFORM == DEF_PLATFORM_LINUX
#    include <sys/eventfd.h>
#endif

#include "time_t.h"
#include "thread_t.h"
#include "queue_t.h"
#include "heap_t.h"
#include "log_t.h"
#include "utility_t.h"
#include "eventIO/internal/posix/eventIO-inl.h"
#include "eventIO/internal/posix/eventIOLoop_t.h"
#include "eventIO/internal/eventTimer_t.h"

static inline bool queuedEvent_init(eventIO_tt* pEventIO)
{
#if DEF_PLATFORM == DEF_PLATFORM_LINUX
    pEventIO->hQueuedEvent[1] = -1;
    pEventIO->hQueuedEvent[0] = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (pEventIO->hQueuedEvent[0] < 0) {
        return false;
    }
#else
    if (pipe(pEventIO->hQueuedEvent) < 0) {
        return false;
    }
    setSocketNonblocking(pEventIO->hQueuedEvent[0]);
    setSocketNonblocking(pEventIO->hQueuedEvent[1]);
#endif
    return true;
}

static inline void queuedEvent_clear(eventIO_tt* pEventIO)
{
    if (pEventIO->hQueuedEvent[0] != -1) {
        close(pEventIO->hQueuedEvent[0]);
        pEventIO->hQueuedEvent[0] = -1;
    }
    if (pEventIO->hQueuedEvent[1] != -1) {
        close(pEventIO->hQueuedEvent[1]);
        pEventIO->hQueuedEvent[1] = -1;
    }
}

static inline void eventIO_doEvents(eventIO_tt* pEventIO)
{
    QUEUE queuePending;
#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIO->queuedLock);
#else
    mutex_lock(&pEventIO->queuedLock);
#endif
    QUEUE_MOVE(&pEventIO->queuedEvent, &queuePending);
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIO->queuedLock);
#else
    mutex_unlock(&pEventIO->queuedLock);
#endif

    eventAsync_tt* pEvent = NULL;
    QUEUE*         pNode  = NULL;
    while (!QUEUE_EMPTY(&queuePending)) {
        pNode = QUEUE_HEAD(&queuePending);
        QUEUE_REMOVE(pNode);

        pEvent = container_of(pNode, eventAsync_tt, node);
        if (atomic_load(&pEventIO->bLoopRunning)) {
            pEvent->fnWork(pEvent);
        }
        else {
            if (pEvent->fnCancel) {
                pEvent->fnCancel(pEvent);
            }
        }
    }
}

typedef struct eventIOStopAsync_s
{
    eventIO_tt*   pEventIO;
    eventAsync_tt eventAsync;
} eventIOStopAsync_tt;

static void inLoop_eventIO_stop(eventAsync_tt* pEventAsync)
{
    eventIOStopAsync_tt* pEventIOStopAsync =
        container_of(pEventAsync, eventIOStopAsync_tt, eventAsync);
    eventIO_tt* pEventIO = pEventIOStopAsync->pEventIO;
    pEventIO->bRunning   = false;
    eventIO_release(pEventIO);
    mem_free(pEventIOStopAsync);
}

static inline void eventIO_loopStop(eventIO_tt* pEventIO)
{
    if (atomic_fetch_sub(&(pEventIO->uiCocurrentRunning), 1) == 1) {
        eventIOStopAsync_tt* pEventIOStopAsync = mem_malloc(sizeof(eventIOStopAsync_tt));
        pEventIOStopAsync->pEventIO            = pEventIO;
        atomic_fetch_add(&(pEventIO->iRefCount), 1);
        eventIO_queueInLoop(
            pEventIO, &pEventIOStopAsync->eventAsync, inLoop_eventIO_stop, inLoop_eventIO_stop);
    }
}

struct eventIOLoop_s* eventIO_connectionLoop(struct eventIO_s* pEventIO)
{
    if (pEventIO->uiCocurrentThreads == 0) {
        atomic_fetch_add(&pEventIO->pEventIOLoop->iConnections, 1);
        return pEventIO->pEventIOLoop;
    }
    else {
        uint32_t uiIndex = 0;
        int32_t  iCount  = atomic_load(&(pEventIO->pEventIOLoop[0].iConnections));
        for (uint32_t i = 1; i < pEventIO->uiCocurrentThreads; ++i) {
            int32_t iThreadConnCount = atomic_load(&pEventIO->pEventIOLoop[i].iConnections);
            if (iThreadConnCount < iCount) {
                iCount  = iThreadConnCount;
                uiIndex = i;
            }
        }
        atomic_fetch_add(&pEventIO->pEventIOLoop[uiIndex].iConnections, 1);
        return &pEventIO->pEventIOLoop[uiIndex];
    }
}

bool eventIO_isInLoopThread(eventIO_tt* pEventIO)
{
    return pEventIO->uiThreadId == threadId();
}

void eventIO_queueInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                         void (*fnWork)(eventAsync_tt*), void (*fnCancel)(eventAsync_tt*))
{
    if (pEventIO->uiCocurrentThreads == 0) {
        eventIOLoop_queueInLoop(pEventIO->pEventIOLoop, pEventAsync, fnWork, fnCancel);
    }
    else {
        pEventAsync->fnWork   = fnWork;
        pEventAsync->fnCancel = fnCancel;
        mutex_lock(&pEventIO->mutex);
        QUEUE_INSERT_TAIL(&pEventIO->queuePending, &pEventAsync->node);
        cond_signal(&pEventIO->cond);
        mutex_unlock(&pEventIO->mutex);
    }
}

void eventIO_runInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                       void (*fnWork)(eventAsync_tt*), void (*fnCancel)(eventAsync_tt*))
{
    if (pEventIO->uiCocurrentThreads == 0) {
        eventIOLoop_runInLoop(pEventIO->pEventIOLoop, pEventAsync, fnWork, fnCancel);
    }
    else {
        if (eventIO_isInLoopThread(pEventIO)) {
            if (atomic_load(&pEventIO->bLoopRunning)) {
                fnWork(pEventAsync);
            }
            else {
                if (fnCancel) {
                    fnCancel(pEventAsync);
                }
            }
        }
        else {
            pEventAsync->fnWork   = fnWork;
            pEventAsync->fnCancel = fnCancel;
            mutex_lock(&pEventIO->mutex);
            QUEUE_INSERT_TAIL(&pEventIO->queuePending, &pEventAsync->node);
            cond_signal(&pEventIO->cond);
            mutex_unlock(&pEventIO->mutex);
        }
    }
}

eventIO_tt* createEventIO()
{
    eventIO_tt* pEventIO = mem_malloc(sizeof(eventIO_tt));
    if (!queuedEvent_init(pEventIO)) {
        mem_free(pEventIO);
        return NULL;
    }
    mutex_init(&pEventIO->mutex);
    QUEUE_INIT(&pEventIO->queuePending);
    atomic_init(&pEventIO->iIdleThreads, 0);
    pEventIO->pEventIOLoop       = NULL;
    pEventIO->uiCocurrentThreads = 0;
    pEventIO->uiLoopTime         = 0;
    pEventIO->uiTimerCounter     = 0;
    pEventIO->uiThreadId         = 0;
    QUEUE_INIT(&pEventIO->queuedEvent);

#ifdef DEF_USE_SPINLOCK
    spinLock_init(&pEventIO->queuedLock);
#else
    mutex_init(&pEventIO->queuedLock);
#endif
    pEventIO->bRunning       = false;
    pEventIO->bTimerEventOff = false;
    cond_init(&pEventIO->cond);
    heap_init((struct heap*)&pEventIO->timerHeap);
    atomic_init(&pEventIO->uiCocurrentRunning, 0);
    atomic_init(&pEventIO->iRefCount, 1);
    atomic_init(&pEventIO->bLoopRunning, false);
    return pEventIO;
}

void eventIO_setConcurrentThreads(eventIO_tt* pEventIO, uint32_t uiCocurrentThreads)
{
    if (!atomic_load(&pEventIO->bLoopRunning)) {
        pEventIO->uiCocurrentThreads = uiCocurrentThreads;
    }
}

int32_t eventIO_getIdleThreads(eventIO_tt* pEventIO)
{
    return atomic_load(&pEventIO->iIdleThreads);
}

uint32_t eventIO_getNumberOfConcurrentThreads(eventIO_tt* pEventIO)
{
    return pEventIO->uiCocurrentThreads;
}

void eventIO_addref(eventIO_tt* pEventIO)
{
    atomic_fetch_add(&(pEventIO->iRefCount), 1);
}

void eventIO_release(eventIO_tt* pEventIO)
{
    if (atomic_fetch_sub(&(pEventIO->iRefCount), 1) == 1) {
        if (pEventIO->pEventIOLoop) {
            mem_free(pEventIO->pEventIOLoop);
            pEventIO->pEventIOLoop = NULL;
        }
        queuedEvent_clear(pEventIO);
        cond_destroy(&pEventIO->cond);
        mutex_destroy(&pEventIO->mutex);
#ifndef DEF_USE_SPINLOCK
        mutex_destroy(&pEventIO->mutex);
#endif
        mem_free(pEventIO);
    }
}

bool eventIO_start(eventIO_tt* pEventIO, bool bTimerEventOff)
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, NULL);

    pEventIO->uiThreadId     = threadId();
    pEventIO->bTimerEventOff = bTimerEventOff;
    atomic_store(&pEventIO->iIdleThreads, 0);
    if (pEventIO->uiCocurrentThreads != 0) {
        atomic_store(&pEventIO->uiCocurrentRunning, pEventIO->uiCocurrentThreads);
        pEventIO->pEventIOLoop = mem_malloc(sizeof(eventIOLoop_tt) * pEventIO->uiCocurrentThreads);
        for (uint32_t i = 0; i < pEventIO->uiCocurrentThreads; ++i) {
            eventIOLoop_init(&(pEventIO->pEventIOLoop[i]), pEventIO, eventIO_loopStop);
            if (!eventIOLoop_start(
                    &(pEventIO->pEventIOLoop[i]), pEventIO->hQueuedEvent[0], eventIO_doEvents)) {
                return false;
            }
            thread_tt thread;
            thread_start(&thread, eventIOLoop_threadRun, &(pEventIO->pEventIOLoop[i]));
        }
    }
    else {
        pEventIO->pEventIOLoop = mem_malloc(sizeof(eventIOLoop_tt));
        eventIOLoop_init(pEventIO->pEventIOLoop, pEventIO, NULL);
        if (!eventIOLoop_start(
                pEventIO->pEventIOLoop, pEventIO->hQueuedEvent[0], eventIO_doEvents)) {
            return false;
        }
    }
    timespec_tt time;
    getClockMonotonic(&time);
    pEventIO->uiLoopTime = timespec_toMsec(&time);
    atomic_fetch_add(&(pEventIO->iRefCount), 1);
    atomic_store(&pEventIO->bLoopRunning, true);
    return true;
}

void eventIO_stop(eventIO_tt* pEventIO)
{
    bool bLoopRunning = true;
    if (atomic_compare_exchange_strong(&pEventIO->bLoopRunning, &bLoopRunning, false)) {
        if (pEventIO->uiCocurrentThreads == 0) {
            eventIOLoop_stop(pEventIO->pEventIOLoop);
        }
        else {
            for (uint32_t i = 0; i < pEventIO->uiCocurrentThreads; ++i) {
                eventIOLoop_stop(&(pEventIO->pEventIOLoop[i]));
            }
        }
    }
}

static inline int32_t eventIO_nextTimeout(const eventIO_tt* pEventIO)
{
    const struct heap_node* pHeapNode = heap_min(&pEventIO->timerHeap);
    if (pHeapNode == NULL) return -1;

    const eventTimer_tt* pHandle = container_of(pHeapNode, eventTimer_tt, node);
    if (pHandle->uiTimeout <= pEventIO->uiLoopTime) return 0;

    return (int32_t)(pHandle->uiTimeout - pEventIO->uiLoopTime);
}

static inline void eventIO_runTimers(eventIO_tt* pEventIO)
{
    struct heap_node* pHeadNode = NULL;
    eventTimer_tt*    pHandle   = NULL;

    for (;;) {
        pHeadNode = heap_min(&pEventIO->timerHeap);
        if (pHeadNode == NULL) break;

        pHandle = container_of(pHeadNode, eventTimer_tt, node);
        if (pHandle->uiTimeout > pEventIO->uiLoopTime) break;
        eventTimer_run(pHandle);
    }
}

static void eventIOLoop_main(eventIOLoop_tt* pEventIOLoop)
{
    pEventIOLoop->uiThreadId = threadId();
    eventIO_tt* pEventIO     = pEventIOLoop->pEventIO;

    timespec_tt time;
    int32_t     iTimeout = -1;
    int32_t     iEvents  = 0;

    while (pEventIOLoop->bRunning) {
        if (!pEventIO->bTimerEventOff) {
            getClockMonotonic(&time);
            pEventIO->uiLoopTime = timespec_toMsec(&time);
            iTimeout             = eventIO_nextTimeout(pEventIO);
        }

        atomic_fetch_add(&pEventIO->iIdleThreads, 1);
        iEvents = poller_wait(pEventIOLoop->pPoller, iTimeout);
        atomic_fetch_sub(&pEventIO->iIdleThreads, 1);
        if (iEvents == -1) {
            break;
        }
        else if (iEvents == 0) {
            pEventIO->uiLoopTime += iTimeout;
            if (!pEventIO->bTimerEventOff) {
                eventIO_runTimers(pEventIO);
            }
        }
        else {
            if (!pEventIO->bTimerEventOff) {
                getClockMonotonic(&time);
                pEventIO->uiLoopTime = timespec_toMsec(&time);
            }
            poller_dispatch(pEventIOLoop->pPoller, iEvents);
        }
    }
    eventIOLoop_clear(pEventIOLoop);
}

void eventIO_dispatch(eventIO_tt* pEventIO)
{
    pEventIO->bRunning   = true;
    pEventIO->uiThreadId = threadId();

    if (pEventIO->uiCocurrentThreads == 0) {
        eventIOLoop_tt* pEventIOLoop = pEventIO->pEventIOLoop;
        eventIOLoop_main(pEventIOLoop);
        pEventIO->bRunning = false;
    }
    else {
        timespec_tt time;
        int32_t     iWaitTimeout = -1;
        QUEUE       queuePending;
        QUEUE_INIT(&queuePending);

        eventAsync_tt* pEvent = NULL;
        QUEUE*         pNode  = NULL;

        getClockMonotonic(&time);
        pEventIO->uiLoopTime = timespec_toMsec(&time);

        while (pEventIO->bRunning) {
            mutex_lock(&pEventIO->mutex);
            if (QUEUE_EMPTY(&pEventIO->queuePending)) {
                if (iWaitTimeout == -1) {
                    cond_wait(&pEventIO->cond, &pEventIO->mutex);
                    QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                    mutex_unlock(&pEventIO->mutex);
                    getClockMonotonic(&time);
                    pEventIO->uiLoopTime = timespec_toMsec(&time);
                }
                else {
                    int32_t iStatus =
                        cond_timedwait(&pEventIO->cond, &pEventIO->mutex, iWaitTimeout * 1e6);
                    switch (iStatus) {
                    case eThreadSuccess:
                    {
                        QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                        mutex_unlock(&pEventIO->mutex);
                        getClockMonotonic(&time);
                        pEventIO->uiLoopTime = timespec_toMsec(&time);
                    } break;
                    case eThreadTimedout:
                    {
                        mutex_unlock(&pEventIO->mutex);
                        pEventIO->uiLoopTime += iWaitTimeout;
                        eventIO_runTimers(pEventIO);
                        getClockMonotonic(&time);
                        pEventIO->uiLoopTime = timespec_toMsec(&time);
                    } break;
                    }
                }
            }
            else {
                QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                mutex_unlock(&pEventIO->mutex);
                getClockMonotonic(&time);
                pEventIO->uiLoopTime = timespec_toMsec(&time);
            }

            if (!QUEUE_EMPTY(&queuePending)) {
                do {
                    pNode = QUEUE_HEAD(&queuePending);
                    QUEUE_REMOVE(pNode);

                    pEvent = container_of(pNode, eventAsync_tt, node);
                    if (atomic_load(&pEventIO->bLoopRunning)) {
                        pEvent->fnWork(pEvent);
                    }
                    else {
                        if (pEvent->fnCancel) {
                            pEvent->fnCancel(pEvent);
                        }
                        else {
                            mem_free(pEvent);
                        }
                    }
                } while (!QUEUE_EMPTY(&queuePending));
            }

            if (!pEventIO->bTimerEventOff) {
                iWaitTimeout = eventIO_nextTimeout(pEventIO);
                if (iWaitTimeout == 0) {
                    eventIO_runTimers(pEventIO);
                    iWaitTimeout = eventIO_nextTimeout(pEventIO);
                }
            }
        }
        mutex_lock(&pEventIO->mutex);
        QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
        mutex_unlock(&pEventIO->mutex);

        while (!QUEUE_EMPTY(&queuePending)) {
            pNode = QUEUE_HEAD(&queuePending);
            QUEUE_REMOVE(pNode);

            pEvent = container_of(pNode, eventAsync_tt, node);
            if (pEvent->fnCancel) {
                pEvent->fnCancel(pEvent);
            }
        }
    }

    QUEUE queuePending;
#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIO->queuedLock);
#else
    mutex_lock(&pEventIO->queuedLock);
#endif
    QUEUE_MOVE(&pEventIO->queuedEvent, &queuePending);
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIO->queuedLock);
#else
    mutex_unlock(&pEventIO->queuedLock);
#endif

    eventAsync_tt* pEvent = NULL;
    QUEUE*         pNode  = NULL;
    while (!QUEUE_EMPTY(&queuePending)) {
        pNode = QUEUE_HEAD(&queuePending);
        QUEUE_REMOVE(pNode);

        pEvent = container_of(pNode, eventAsync_tt, node);
        if (pEvent->fnCancel) {
            pEvent->fnCancel(pEvent);
        }
    }
    eventIO_release(pEventIO);
}
