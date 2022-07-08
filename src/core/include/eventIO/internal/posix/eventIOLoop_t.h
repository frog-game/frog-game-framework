#pragma once

#include "eventIO/internal/posix/eventIO-inl.h"

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

#define TEST_ERR_RW_RETRIABLE(e) ((e) == EINTR || (e) == EAGAIN)

static inline void setSocketNonblocking(int32_t hSocket)
{
    int32_t iFlags = fcntl(hSocket, F_GETFL, 0);
    fcntl(hSocket, F_SETFL, iFlags | O_NONBLOCK);
}

struct eventIOLoop_s;

typedef struct wakeupEvent_s
{
    void (*fn)(struct eventIOLoop_s*);
    int32_t               hWakeupFd[2];
    pollHandle_tt         pollHandle;
    atomic_bool           bNotified;
    struct eventIOLoop_s* pEventIOLoop;
} wakeupEvent_tt;

static inline bool wakeupEvent_init(wakeupEvent_tt* pWakeupEvent)
{
#if DEF_PLATFORM == DEF_PLATFORM_LINUX
    pWakeupEvent->hWakeupFd[1] = -1;
    pWakeupEvent->hWakeupFd[0] = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (pWakeupEvent->hWakeupFd[0] < 0) {
        return false;
    }
#else
    if (pipe(pWakeupEvent->hWakeupFd) < 0) {
        return false;
    }
    setSocketNonblocking(pWakeupEvent->hWakeupFd[0]);
    setSocketNonblocking(pWakeupEvent->hWakeupFd[1]);
#endif
    pWakeupEvent->pEventIOLoop = NULL;
    pWakeupEvent->fn           = NULL;
    pollHandle_init(&pWakeupEvent->pollHandle);
    atomic_init(&pWakeupEvent->bNotified, false);
    return true;
}

static inline void wakeupEvent_handleEvent(struct pollHandle_s* pHandle, int32_t iEvents)
{
    wakeupEvent_tt* pWakeupEvent = container_of(pHandle, wakeupEvent_tt, pollHandle);
    if (iEvents & ePollerReadable) {
        char szBuf[128];
        if (read(pWakeupEvent->hWakeupFd[0], szBuf, sizeof(szBuf)) > 0) {
            atomic_store(&pWakeupEvent->bNotified, false);
            pWakeupEvent->fn(pWakeupEvent->pEventIOLoop);
        }
    }
}

static bool wakeupEvent_start(wakeupEvent_tt* pWakeupEvent, struct eventIOLoop_s* pEventIOLoop,
                              poller_tt* pPoller, void (*fn)(struct eventIOLoop_s*))
{
    pWakeupEvent->pEventIOLoop = pEventIOLoop;
    pWakeupEvent->fn           = fn;
    poller_add(pPoller,
               pWakeupEvent->hWakeupFd[0],
               &pWakeupEvent->pollHandle,
               ePollerReadable,
               wakeupEvent_handleEvent);
    return true;
}

static inline void wakeupEvent_notify(wakeupEvent_tt* pWakeupEvent)
{
    if (!atomic_load(&pWakeupEvent->bNotified)) {
        atomic_store(&pWakeupEvent->bNotified, true);

        uint64_t once = 1;
#if DEF_PLATFORM == DEF_PLATFORM_LINUX
        write(pWakeupEvent->hWakeupFd[0], &once, sizeof once);
#else
        write(pWakeupEvent->hWakeupFd[1], &once, sizeof once);
#endif
    }
}

static inline void wakeupEvent_clear(wakeupEvent_tt* pWakeupEvent, poller_tt* pPoller)
{
    atomic_store(&pWakeupEvent->bNotified, true);
    if (pWakeupEvent->hWakeupFd[0] != -1) {
        if (pPoller) {
            poller_clear(
                pPoller, pWakeupEvent->hWakeupFd[0], &pWakeupEvent->pollHandle, ePollerClosed);
        }
        close(pWakeupEvent->hWakeupFd[0]);
        pWakeupEvent->hWakeupFd[0] = -1;
    }

    if (pWakeupEvent->hWakeupFd[1] != -1) {
        close(pWakeupEvent->hWakeupFd[1]);
        pWakeupEvent->hWakeupFd[1] = -1;
    }
}

struct eventIO_s;

typedef struct eventIOLoop_s
{
    void (*fnStop)(struct eventIO_s*);
    void (*fnDoEvents)(struct eventIO_s*);
    struct eventIO_s* pEventIO;
    poller_tt*        pPoller;
    wakeupEvent_tt    wakeupEvent;
    QUEUE             queuePending;
    int32_t           hQueuedEvent;
    pollHandle_tt     queuedHandle;
    uint64_t          uiThreadId;
    bool              bRunning;
    atomic_int        iConnections;
#ifdef DEF_USE_SPINLOCK
    spinLock_tt spinLock;
#else
    mutex_tt mutex;
#endif
} eventIOLoop_tt;

static inline bool eventIOLoop_isInLoopThread(eventIOLoop_tt* pEventIOLoop)
{
    return pEventIOLoop->uiThreadId == threadId();
}

static inline void eventIOLoop_queueInLoop(eventIOLoop_tt* pEventIOLoop, eventAsync_tt* pEventAsync,
                                           void (*fnWork)(eventAsync_tt*),
                                           void (*fnCancel)(eventAsync_tt*))
{
    pEventAsync->fnWork   = fnWork;
    pEventAsync->fnCancel = fnCancel;

#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIOLoop->spinLock);
#else
    mutex_lock(&pEventIOLoop->mutex);
#endif
    QUEUE_INSERT_TAIL(&pEventIOLoop->queuePending, &pEventAsync->node);
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIOLoop->spinLock);
#else
    mutex_unlock(&pEventIOLoop->mutex);
#endif
    wakeupEvent_notify(&pEventIOLoop->wakeupEvent);
}

static inline void eventIOLoop_runInLoop(eventIOLoop_tt* pEventIOLoop, eventAsync_tt* pEventAsync,
                                         void (*fnWork)(eventAsync_tt*),
                                         void (*fnCancel)(eventAsync_tt*))
{
    if (eventIOLoop_isInLoopThread(pEventIOLoop)) {
        if (eventIO_isRunning(pEventIOLoop->pEventIO)) {
            fnWork(pEventAsync);
        }
        else {
            if (fnCancel) {
                fnCancel(pEventAsync);
            }
        }
    }
    else {
        eventIOLoop_queueInLoop(pEventIOLoop, pEventAsync, fnWork, fnCancel);
    }
}

__UNUSED void eventIOLoop_init(eventIOLoop_tt* pEventIOLoop, struct eventIO_s* pEventIO,
                               void (*fnStop)(struct eventIO_s*));

__UNUSED void eventIOLoop_clear(eventIOLoop_tt* pEventIOLoop);

__UNUSED bool eventIOLoop_start(eventIOLoop_tt* pEventIOLoop, int32_t hQueuedEvent,
                                void (*fnDoEvents)(struct eventIO_s*));

__UNUSED void eventIOLoop_stop(eventIOLoop_tt* pEventIOLoop);

__UNUSED void eventIOLoop_threadRun(void* pArg);
