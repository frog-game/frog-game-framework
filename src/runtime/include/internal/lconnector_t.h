

#pragma once

// type
#include <stdint.h>

#include "utility_t.h"

struct lua_State;
struct connector_s;

typedef struct lconnector_s
{
    struct connector_s* pHandle;
} lconnector_tt;

__UNUSED int32_t registerConnectorL(struct lua_State* L);
