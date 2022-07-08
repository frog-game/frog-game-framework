#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <stdatomic.h>

#include "utility_t.h"

enum enPollerAttr
{
    ePollerReadable = 0x01,
    ePollerWritable = 0x02,
    ePollerClosed   = 0x04
};

struct poller_s;
struct pollHandle_s;

typedef struct poller_s     poller_tt;
typedef struct pollHandle_s pollHandle_tt;

typedef void (*pollCallbackFunc)(pollHandle_tt*, int32_t);

struct pollHandle_s
{
    // private
    int32_t          iAttribute;
    pollCallbackFunc fn;
};

static inline void pollHandle_init(pollHandle_tt* pPollHandle)
{
    pPollHandle->iAttribute = ePollerClosed;
    pPollHandle->fn         = NULL;
}

static inline bool pollHandle_isWriting(pollHandle_tt* pPollHandle)
{
    return pPollHandle->iAttribute & ePollerWritable;
}

static inline bool pollHandle_isReading(pollHandle_tt* pPollHandle)
{
    return pPollHandle->iAttribute & ePollerReadable;
}

static inline bool pollHandle_isClosed(pollHandle_tt* pPollHandle)
{
    return pPollHandle->iAttribute & ePollerClosed;
}

__UNUSED poller_tt* createPoller();

__UNUSED void poller_release(poller_tt* pPoller);

__UNUSED int32_t poller_wait(poller_tt* pPoller, int32_t iTimeoutMs);

__UNUSED void poller_dispatch(poller_tt* pPoller, int32_t iEvents);

__UNUSED void poller_add(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle,
                         int32_t iAttribute, pollCallbackFunc fn);

__UNUSED void poller_clear(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle,
                           int32_t iAttribute);

__UNUSED void poller_setOpt(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle,
                            int32_t iAttribute);