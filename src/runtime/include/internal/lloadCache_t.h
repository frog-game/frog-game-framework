

#pragma once

#include <stdint.h>

#include "utility_t.h"

struct lua_State;

__UNUSED void luaCache_init();

__UNUSED void luaCache_clear();

__UNUSED void luaCache_on();

__UNUSED void luaCache_off();

__UNUSED bool luaCache_abandon(const char* szFileName);

__UNUSED int32_t loadfileCache(struct lua_State* L, const char* szFileName);

__UNUSED int32_t loadCache(struct lua_State* L);
