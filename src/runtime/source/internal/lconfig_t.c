

#include "internal/lconfig_t.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "internal/lpackagePath_t.h"
#include "log_t.h"
#include "platform_t.h"

static void* lua_config_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;
    if (nsize == 0) {
        mem_free(ptr);
        return NULL;
    }
    else
        return mem_realloc(ptr, nsize);
}

typedef struct luaConfig_s
{
    char*   szLoaderPath;
    char*   szServicePath;
    char*   szLogPath;
    char*   szLogService;
    char*   szBootstrap;
    char*   szBootstrapParam;
    char*   szDebug_ip;
    char*   szDebug_port;
    int32_t iServerNodeId;
    int32_t iConcurrentThreads;
    bool    bLog;
    bool    bProfile;
} luaConfig_tt;

static luaConfig_tt* s_pLuaConfig = NULL;

static void luaConfig_setLogPath(const char* szPath)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szLogPath != NULL) {
            mem_free(s_pLuaConfig->szLogPath);
            s_pLuaConfig->szLogPath = NULL;
        }

        s_pLuaConfig->szLogPath = mem_strdup(szPath);
    }
}

static void luaConfig_setLoaderPath(const char* szPath)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szLoaderPath != NULL) {
            mem_free(s_pLuaConfig->szLoaderPath);
            s_pLuaConfig->szLoaderPath = NULL;
        }

        s_pLuaConfig->szLoaderPath = mem_strdup(szPath);
    }
}

static void luaConfig_set_debug_ip(const char* debug_ip)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szDebug_ip != NULL) {
            mem_free(s_pLuaConfig->szDebug_ip);
            s_pLuaConfig->szDebug_ip = NULL;
        }

        s_pLuaConfig->szDebug_ip = mem_strdup(debug_ip);
    }
}

static void luaConfig_set_debug_port(const char* debug_port)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szDebug_port != NULL) {
            mem_free(s_pLuaConfig->szDebug_port);
            s_pLuaConfig->szDebug_port = NULL;
        }

        s_pLuaConfig->szDebug_port = mem_strdup(debug_port);
    }
}

static void luaConfig_setServicePath(const char* szPath)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szServicePath != NULL) {
            mem_free(s_pLuaConfig->szServicePath);
            s_pLuaConfig->szServicePath = NULL;
        }

        s_pLuaConfig->szServicePath = mem_strdup(szPath);
    }
}

static void luaConfig_setBootstrap(const char* szName)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szBootstrap != NULL) {
            mem_free(s_pLuaConfig->szBootstrap);
            s_pLuaConfig->szBootstrap = NULL;
        }

        s_pLuaConfig->szBootstrap = mem_strdup(szName);
    }
}

static void luaConfig_setBootstrapParam(const char* szParam)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szBootstrapParam != NULL) {
            mem_free(s_pLuaConfig->szBootstrapParam);
            s_pLuaConfig->szBootstrapParam = NULL;
        }
        s_pLuaConfig->szBootstrapParam = mem_strdup(szParam);
    }
}

static void luaConfig_setLogService(const char* szName)
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szLogService != NULL) {
            mem_free(s_pLuaConfig->szLogService);
            s_pLuaConfig->szLogService = NULL;
        }

        s_pLuaConfig->szLogService = mem_strdup(szName);
    }
}

bool luaConfig_init(lua_State* L)
{
    if (s_pLuaConfig != NULL) {
        return false;
    }

    int32_t iCount = lua_gettop(L);
    if (iCount != 1) {
        return false;
    }

    int32_t iType = lua_type(L, 1);
    if (iType != LUA_TSTRING) {
        return false;
    }

    const char* szConfigLuaFile = lua_tostring(L, 1);

    lua_State* pLuaState = lua_newstate(lua_config_alloc, NULL);
    luaL_openlibs(pLuaState);

    if (luaL_loadfile(pLuaState, szConfigLuaFile)) {
        Log(eLog_fatal, "loadfile:%s error:%s", szConfigLuaFile, lua_tostring(pLuaState, -1));
        return false;
    }
    lua_pushinteger(pLuaState, DEF_PLATFORM);
    if (lua_pcall(pLuaState, 1, 0, 0)) {
        Log(eLog_fatal,
            "PANIC: unprotected error in call to Lua API error:%s",
            lua_tostring(pLuaState, -1));
        return false;
    }

    s_pLuaConfig                     = mem_malloc(sizeof(luaConfig_tt));
    s_pLuaConfig->szLoaderPath       = NULL;
    s_pLuaConfig->szServicePath      = NULL;
    s_pLuaConfig->szLogPath          = NULL;
    s_pLuaConfig->szBootstrap        = NULL;
    s_pLuaConfig->szBootstrapParam   = NULL;
    s_pLuaConfig->szLogService       = NULL;
    s_pLuaConfig->szDebug_ip         = NULL;
    s_pLuaConfig->szDebug_port       = NULL;
    s_pLuaConfig->iServerNodeId      = 0;
    s_pLuaConfig->iConcurrentThreads = 0;
    s_pLuaConfig->bProfile           = false;
    s_pLuaConfig->bLog               = false;

    lua_getglobal(pLuaState, "C_loader_path");
    const char* szLoaderPath = lua_tostring(pLuaState, -1);
    luaConfig_setLoaderPath(szLoaderPath);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_debug_ip");
    const char* szDebug_ip = lua_tostring(pLuaState, -1);
    luaConfig_set_debug_ip(szDebug_ip);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_debug_port");
    const char* szDebug_port = lua_tostring(pLuaState, -1);
    luaConfig_set_debug_port(szDebug_port);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_service_path");
    const char* szServicePath = lua_tostring(pLuaState, -1);
    luaConfig_setServicePath(szServicePath);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_log_path");
    const char* szLogPath = lua_tostring(pLuaState, -1);
    luaConfig_setLogPath(szLogPath);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_bootstrap");
    const char* szBootstrap = lua_tostring(pLuaState, -1);
    luaConfig_setBootstrap(szBootstrap);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_bootstrap_param");
    const char* szBootstrapParam = lua_tostring(pLuaState, -1);
    luaConfig_setBootstrapParam(szBootstrapParam);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_log_name");
    const char* szLogService = lua_tostring(pLuaState, -1);
    luaConfig_setLogService(szLogService);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_node_id");
    s_pLuaConfig->iServerNodeId = (int32_t)lua_tointeger(pLuaState, -1);
    s_pLuaConfig->iServerNodeId &= 0x7ff;
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_concurrent_threads");
    s_pLuaConfig->iConcurrentThreads = (int32_t)lua_tointeger(pLuaState, -1);
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_log");
    s_pLuaConfig->bLog = lua_toboolean(pLuaState, 1) ? true : false;
    lua_pop(pLuaState, 1);

    lua_getglobal(pLuaState, "C_profile");
    s_pLuaConfig->bProfile = lua_toboolean(pLuaState, 1) ? true : false;
    lua_pop(pLuaState, 1);

    lua_close(pLuaState);
    return true;
}

void luaConfig_clear()
{
    if (s_pLuaConfig != NULL) {
        if (s_pLuaConfig->szLoaderPath) {
            mem_free(s_pLuaConfig->szLoaderPath);
            s_pLuaConfig->szLoaderPath = NULL;
        }

        if (s_pLuaConfig->szDebug_ip) {
            mem_free(s_pLuaConfig->szDebug_ip);
            s_pLuaConfig->szDebug_ip = NULL;
        }

        if (s_pLuaConfig->szDebug_port) {
            mem_free(s_pLuaConfig->szDebug_port);
            s_pLuaConfig->szDebug_port = NULL;
        }

        if (s_pLuaConfig->szServicePath) {
            mem_free(s_pLuaConfig->szServicePath);
            s_pLuaConfig->szServicePath = NULL;
        }

        if (s_pLuaConfig->szLogPath) {
            mem_free(s_pLuaConfig->szLogPath);
            s_pLuaConfig->szLogPath = NULL;
        }

        if (s_pLuaConfig->szBootstrap) {
            mem_free(s_pLuaConfig->szBootstrap);
            s_pLuaConfig->szBootstrap = NULL;
        }

        if (s_pLuaConfig->szBootstrapParam) {
            mem_free(s_pLuaConfig->szBootstrapParam);
            s_pLuaConfig->szBootstrapParam = NULL;
        }

        if (s_pLuaConfig->szLogService) {
            mem_free(s_pLuaConfig->szLogService);
            s_pLuaConfig->szLogService = NULL;
        }

        mem_free(s_pLuaConfig);
        s_pLuaConfig = NULL;
    }
}

const char* luaConfig_getServicePath()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szServicePath != NULL) {
        return s_pLuaConfig->szServicePath;
    }
    return "./";
}

const char* luaConfig_getLoaderPath()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szLoaderPath != NULL) {
        return s_pLuaConfig->szLoaderPath;
    }
    return "./";
}

const char* luaConfig_getDebug_ip()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szDebug_ip != NULL) {
        return s_pLuaConfig->szDebug_ip;
    }
    return "127.0.0.1";
}

const char* luaConfig_getDebug_port()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szDebug_port != NULL) {
        return s_pLuaConfig->szDebug_port;
    }
    return "9966";
}

const char* luaConfig_getLogPath()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szLogPath != NULL) {
        return s_pLuaConfig->szLogPath;
    }
    return "tmp";
}

const char* luaConfig_getBootstrap()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szBootstrap != NULL) {
        return s_pLuaConfig->szBootstrap;
    }
    return "bootstrap";
}

const char* luaConfig_getLogService()
{
    assert(s_pLuaConfig);
    if (s_pLuaConfig->szLogService != NULL) {
        return s_pLuaConfig->szLogService;
    }
    return "log";
}

const char* luaConfig_getBootstrapParam()
{
    assert(s_pLuaConfig);
    return s_pLuaConfig->szBootstrapParam;
}

int32_t luaConfig_getServerNodeID()
{
    assert(s_pLuaConfig);
    return s_pLuaConfig->iServerNodeId;
}

int32_t luaConfig_getConcurrentThreads()
{
    assert(s_pLuaConfig);
    return s_pLuaConfig->iConcurrentThreads;
}

bool luaConfig_isProfile()
{
    assert(s_pLuaConfig);
    return s_pLuaConfig->bProfile;
}

bool luaConfig_isLog()
{
    assert(s_pLuaConfig);
    return s_pLuaConfig->bLog;
}