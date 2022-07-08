#pragma once

#include <stdbool.h>
#include <stdatomic.h>

#include "eventIO/internal/posix/poller_t.h"
#include "eventIO/eventAsync_t.h"

typedef struct eventWatcherAsync_s
{
    eventAsync_tt          eventAsync;
    struct eventWatcher_s* pEventWatcher;
} eventWatcherAsync_tt;

struct eventWatcher_s
{
    void (*fn)(struct eventWatcher_s*, void*);
    void (*fnUserFree)(void*);
    struct eventIO_s*    pEventIO;
    void*                pUserData;
    bool                 bManualReset;
    atomic_int           iStatus;
    atomic_int           iRefCount;
    eventWatcherAsync_tt notifiedEventAsync;
};