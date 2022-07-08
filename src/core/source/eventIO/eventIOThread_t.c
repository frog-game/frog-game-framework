#include "eventIO/eventIOThread_t.h"
#include <stdatomic.h>
#include "utility_t.h"
#include "thread_t.h"

enum enStatus
{
    eStarting,
    eRunning,
    eStopping,
    eStopped
};

struct eventIOThread_s
{
    bool (*fnPre)(eventIOThread_tt*);
    void (*fnPost)(eventIOThread_tt*);
    mutex_tt    mutex;
    eventIO_tt* pEventIO;
    thread_tt   thread;
    atomic_int  iStatus;
    atomic_int  iRefCount;
};

void eventIOThread_release(eventIOThread_tt* pEventIOThread);

static inline void eventIOThreadRun(void* pArg)
{
    eventIOThread_tt* pEventIOThread = (eventIOThread_tt*)pArg;

    int32_t iStatus = eStarting;
    if (atomic_compare_exchange_strong(&pEventIOThread->iStatus, &iStatus, eRunning)) {
        if (pEventIOThread->fnPre) {
            if (!pEventIOThread->fnPre(pEventIOThread)) {
                eventIO_stop(pEventIOThread->pEventIO);
            }
        }
        eventIO_dispatch(pEventIOThread->pEventIO);
    }

    if (pEventIOThread->fnPost) {
        pEventIOThread->fnPost(pEventIOThread);
    }

    atomic_store(&pEventIOThread->iStatus, eStopped);
    eventIOThread_release(pEventIOThread);
}

eventIOThread_tt* createEventIOThread(eventIO_tt* pEventIO)
{
    eventIOThread_tt* pEventIOThread = mem_malloc(sizeof(eventIOThread_tt));
    pEventIOThread->pEventIO         = pEventIO;
    eventIO_addref(pEventIOThread->pEventIO);
    pEventIOThread->fnPre  = NULL;
    pEventIOThread->fnPost = NULL;
    mutex_init(&pEventIOThread->mutex);
    atomic_init(&pEventIOThread->iStatus, eStopped);
    atomic_init(&pEventIOThread->iRefCount, 1);
    return pEventIOThread;
}

void eventIOThread_addref(eventIOThread_tt* pEventIOThread)
{
    atomic_fetch_add(&(pEventIOThread->iRefCount), 1);
}

void eventIOThread_release(eventIOThread_tt* pEventIOThread)
{
    if (atomic_fetch_sub(&(pEventIOThread->iRefCount), 1) == 1) {
        if (pEventIOThread->pEventIO) {
            eventIO_release(pEventIOThread->pEventIO);
            pEventIOThread->pEventIO = NULL;
        }
        mutex_destroy(&pEventIOThread->mutex);
        mem_free(pEventIOThread);
    }
}

void eventIOThread_start(eventIOThread_tt* pEventIOThread, bool bWaitThreadStarted /*= true */,
                         bool (*fnPre)(eventIOThread_tt*), void (*fnPost)(eventIOThread_tt*))
{
    atomic_store(&pEventIOThread->iStatus, eStarting);

    atomic_fetch_add(&(pEventIOThread->iRefCount), 1);
    thread_start(&pEventIOThread->thread, eventIOThreadRun, pEventIOThread);

    if (bWaitThreadStarted) {
        while (atomic_load(&pEventIOThread->iStatus) < eRunning) {
            threadYield();
        }

        if (eventIO_getNumberOfConcurrentThreads(pEventIOThread->pEventIO) == 0) {
            while (atomic_load(&pEventIOThread->iStatus) == eRunning) {
                if (eventIO_getIdleThreads(pEventIOThread->pEventIO) == 1) {
                    break;
                }
                else {
                    threadYield();
                }
            }
        }
        else {
            while (atomic_load(&pEventIOThread->iStatus) == eRunning) {
                if (eventIO_getIdleThreads(pEventIOThread->pEventIO) ==
                    eventIO_getNumberOfConcurrentThreads(pEventIOThread->pEventIO)) {
                    break;
                }
                else {
                    threadYield();
                }
            }
        }
    }
}

void eventIOThread_join(eventIOThread_tt* pEventIOThread)
{
    if (atomic_load(&pEventIOThread->iStatus) != eStopped) {
        mutex_lock(&pEventIOThread->mutex);
        thread_join(pEventIOThread->thread);
        mutex_unlock(&pEventIOThread->mutex);
    }
}

void eventIOThread_stop(eventIOThread_tt* pEventIOThread, bool bWaitThreadStarted /*=false */)
{
    int32_t iStatus = eStarting;
    if (atomic_compare_exchange_strong(&pEventIOThread->iStatus, &iStatus, eStopping)) {
        eventIO_stop(pEventIOThread->pEventIO);
        if (bWaitThreadStarted) {
            eventIOThread_join(pEventIOThread);
        }
    }
    else if (iStatus == eRunning) {
        if (atomic_compare_exchange_strong(&pEventIOThread->iStatus, &iStatus, eStopping)) {
            eventIO_stop(pEventIOThread->pEventIO);
            if (bWaitThreadStarted) {
                eventIOThread_join(pEventIOThread);
            }
        }
    }
}

bool eventIOThread_isRunning(eventIOThread_tt* pEventIOThread)
{
    return atomic_load(&pEventIOThread->iStatus) == eRunning;
}

bool eventIOThread_isStopped(eventIOThread_tt* pEventIOThread)
{
    return atomic_load(&pEventIOThread->iStatus) == eStopped;
}

bool eventIOThread_isStopping(eventIOThread_tt* pEventIOThread)
{
    return atomic_load(&pEventIOThread->iStatus) == eStopping;
}

eventIO_tt* eventIOThread_getEventIO(eventIOThread_tt* pEventIOThread)
{
    return pEventIOThread->pEventIO;
}