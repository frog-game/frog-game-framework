

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "eventIO/eventIO_t.h"

struct service_s;

struct listenPort_s
{
    void (*fn)(struct service_s*, eventConnection_tt*, void*);
    void (*fnFailCallback)(struct service_s*, void*);
    struct service_s*   pService;
    eventListenPort_tt* pListenPortHandle;
    atomic_int          iRefCount;
};
