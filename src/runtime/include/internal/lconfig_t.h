

#pragma once

// type
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utility_t.h"

struct lua_State;

__UNUSED bool luaConfig_init(struct lua_State* L);

__UNUSED void luaConfig_clear();

__UNUSED const char* luaConfig_getLoaderPath();

__UNUSED const char* luaConfig_getServicePath();

__UNUSED const char* luaConfig_getLogPath();

__UNUSED const char* luaConfig_getBootstrap();

__UNUSED const char* luaConfig_getBootstrapParam();

__UNUSED const char* luaConfig_getLogService();

__UNUSED int32_t luaConfig_getServerNodeID();

__UNUSED int32_t luaConfig_getConcurrentThreads();

__UNUSED bool luaConfig_isLog();

__UNUSED bool luaConfig_isProfile();

__UNUSED const char* luaConfig_getDebug_ip();

__UNUSED const char* luaConfig_getDebug_port();
