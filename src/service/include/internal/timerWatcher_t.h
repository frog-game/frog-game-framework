

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "eventIO/eventIO_t.h"

struct timerWatcher_s
{
    struct service_s* pService;
    uint32_t          uiToken;
    eventTimer_tt*    pEventTimer;
    atomic_int        iRefCount;
};
