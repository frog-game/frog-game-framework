

#pragma once

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "queue_t.h"
#include "heap_t.h"
#include "thread_t.h"
#include "log_t.h"
#include "spinLock_t.h"

#define DEF_USE_SPINLOCK

struct eventIO_s
{
    HANDLE      hCompletionPort;
    uint32_t    uiCocurrentThreads;
    uint64_t    uiLoopTime;
    uint64_t    uiTimerCounter;
    struct heap timerHeap;
    bool        bTimerEventOff;
    QUEUE       queueSocketReuse;
#ifdef DEF_USE_SPINLOCK
    spinLock_tt socketReuseLock;
#else
    mutex_tt socketReuseLock;
#endif
    atomic_uint uiCocurrentRunning;
    atomic_int  iIdleThreads;
    QUEUE       queuePending;
    mutex_tt    mutex;
    uint64_t    uiThreadId;
    bool        bRunning;
    bool        bLoopSleep;
    atomic_bool bLoopRunning;
    atomic_bool bLoopNotified;
    cond_tt     cond;
    atomic_int  iRefCount;
};

static inline bool eventIO_isRunning(struct eventIO_s* pEventIO)
{
    return atomic_load(&pEventIO->bLoopRunning);
}

__UNUSED SOCKET eventIO_makeSocket(struct eventIO_s* pEventIO);

__UNUSED void eventIO_recoverySocket(struct eventIO_s* pEventIO, SOCKET hSocket);
