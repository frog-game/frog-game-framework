

#pragma once

// type
#include <stdint.h>

#include "utility_t.h"

struct lua_State;
struct dnsResolve_s;

typedef struct ldnsResolve_s
{
    struct dnsResolve_s* pHandle;
} ldnsResolve_tt;

__UNUSED int32_t registerDnsResolveL(struct lua_State* L);
