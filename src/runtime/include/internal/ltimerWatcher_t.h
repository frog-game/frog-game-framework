

#pragma once

#include <stdint.h>

#include "utility_t.h"

struct lua_State;
struct timerWatcher_s;

typedef struct ltimerWatcher_s
{
    struct timerWatcher_s* pHandle;
} ltimerWatcher_tt;

__UNUSED int32_t registerTimerWatcherL(struct lua_State* L);
