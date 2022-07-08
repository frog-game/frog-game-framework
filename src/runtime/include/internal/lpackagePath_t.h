

#pragma once

#include "utility_t.h"

struct lua_State;

__UNUSED void setPackage_path(struct lua_State* L, const char* szPackagePath);

__UNUSED void setPackage_cpath(struct lua_State* L, const char* szPackagePath);