

#pragma once

#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>

#include "eventIO/eventAsync_t.h"

struct eventWatcher_s;

typedef struct eventWatcher_overlappedPlus_s
{
    void (*fn)(struct eventWatcher_s*);
    OVERLAPPED             _Overlapped;
    struct eventWatcher_s* pEventWatcher;
} eventWatcher_overlappedPlus_tt;

struct eventWatcher_s
{
    void (*fn)(struct eventWatcher_s*, void*);
    void (*fnUserFree)(void*);
    struct eventIO_s*              pEventIO;
    void*                          pUserData;
    bool                           bManualReset;
    atomic_int                     iStatus;
    atomic_int                     iRefCount;
    eventWatcher_overlappedPlus_tt notifiedEvent;
};
