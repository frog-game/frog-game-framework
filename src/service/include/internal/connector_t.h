

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "eventIO/eventIO_t.h"

struct connector_s
{
    struct service_s*            pService;
    uint32_t                     uiToken;
    _Atomic(eventConnection_tt*) hConnection;
    _Atomic(eventTimer_tt*)      hConnectingTimeout;
    atomic_int                   iRefCount;
};
