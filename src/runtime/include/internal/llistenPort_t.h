

#pragma once

// type
#include <stdint.h>

#include "utility_t.h"

struct lua_State;
struct listenPort_s;

typedef struct llistenPort_s
{
    struct listenPort_s* pHandle;
} llistenPort_tt;

__UNUSED int32_t registerListenPortL(struct lua_State* L);
