

#include "env/lenv_t.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "log_t.h"
#include "thread_t.h"
#include "utility_t.h"

#include "eventIO/eventIOThread_t.h"
#include "openssl/crypt_t.h"

#include "channel/channelCenter_t.h"
#include "serviceCenter_t.h"
#include "serviceMonitor_t.h"
#include "service_t.h"

#include "internal/lconfig_t.h"
#include "internal/lloadCache_t.h"
#include "internal/lpackagePath_t.h"

#include "internal/lservice-inl.h"

static eventIOThread_tt* s_pEventIOThread = NULL;

eventIO_tt* getEnvEventIO()
{
    return eventIOThread_getEventIO(s_pEventIOThread);
}

static void inLoop_bootstrap_start(eventAsync_tt* pEventAsync)
{
    size_t      nLength = 0;
    const char* szParam = luaConfig_getBootstrapParam();
    if (szParam) {
        nLength = strlen(szParam);
    }
    if (createLuaService(luaConfig_getBootstrap(), szParam, nLength) == 0) {
        Log(eLog_error, "bootstrap service create error");
        if (s_pEventIOThread) {
            eventIOThread_stop(s_pEventIOThread, false);
        }
    }
    mem_free(pEventAsync);
}

static void inLoop_bootstrap_stop(eventAsync_tt* pEventAsync)
{
    mem_free(pEventAsync);
}

static int32_t lenv_init(lua_State* L)
{
    if (!luaConfig_init(L)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    eventIO_tt* pEventIO = createEventIO();
    if (luaConfig_getConcurrentThreads() == -1) {
        eventIO_setConcurrentThreads(pEventIO, threadHardwareConcurrency() * 2);
    }
    else if (luaConfig_getConcurrentThreads() == 0) {
        eventIO_setConcurrentThreads(pEventIO, threadHardwareConcurrency());
    }
    else {
        eventIO_setConcurrentThreads(pEventIO, luaConfig_getConcurrentThreads());
    }
    eventIO_start(pEventIO, false);

    s_pEventIOThread = createEventIOThread(pEventIO);
    eventIOThread_start(s_pEventIOThread, true, NULL, NULL);

    dnsStartup();
    serviceCenter_init(luaConfig_getServerNodeID());
    serviceMonitor_init(eventIO_getNumberOfConcurrentThreads(pEventIO));
    channelCenter_init();
    luaCache_init();
    eventAsync_tt* pEventAsync = mem_malloc(sizeof(eventAsync_tt));
    eventIO_queueInLoop(pEventIO, pEventAsync, inLoop_bootstrap_start, inLoop_bootstrap_stop);
    eventIO_release(pEventIO);
    lua_pushboolean(L, true);
    return 1;
}

static int32_t lenv_wait(lua_State* L)
{
    if (s_pEventIOThread) {
        eventIOThread_join(s_pEventIOThread);
    }
    return 0;
}

static int32_t lenv_stop(lua_State* L)
{
    if (s_pEventIOThread) {
        eventIOThread_stop(s_pEventIOThread, true);
    }
    return 0;
}

static int32_t lenv_exit(lua_State* L)
{
    if (s_pEventIOThread) {
        eventIOThread_release(s_pEventIOThread);
        s_pEventIOThread = NULL;
    }

    channelCenter_clear();
    serviceMonitor_clear();
    serviceCenter_clear();
    luaCache_clear();
    dnsCleanup();
    luaConfig_clear();
    return 0;
}

static int32_t lenv_monitorStart(lua_State* L)
{
    uint32_t    serviceID      = (uint32_t)luaL_checkinteger(L, 1);
    bool        bSucc          = false;
    service_tt* pServiceHandle = serviceCenter_gain(serviceID);
    if (pServiceHandle) {
        uint32_t uiIntervalMs = (uint32_t)luaL_checkinteger(L, 2);
        bSucc                 = serviceMonitor_start(
            serviceID, eventIOThread_getEventIO(s_pEventIOThread), uiIntervalMs);
        service_release(pServiceHandle);
    }
    lua_pushboolean(L, bSucc ? 1 : 0);
    return 1;
}

static int32_t lenv_monitorStop(lua_State* L)
{
    serviceMonitor_stop();
    return 0;
}

static int32_t lenv_monitorWaitForCount(lua_State* L)
{
    lua_pushinteger(L, serviceMonitor_waitForCount());
    return 1;
}

static int32_t lenv_luacacheOn(lua_State* L)
{
    luaCache_on();
    return 0;
}

static int32_t lenv_luacacheOff(lua_State* L)
{
    luaCache_off();
    return 0;
}

static int32_t lenv_luacacheAbandon(lua_State* L)
{
    const char* szFileName = luaL_checkstring(L, 1);
    lua_pushboolean(L, luaCache_abandon(szFileName));
    return 1;
}

int32_t luaopen_lruntime_env(lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    crypto_customize_mem_init();

    luaL_Reg lualib_env[] = {{"init", lenv_init},
                             {"wait", lenv_wait},
                             {"stop", lenv_stop},
                             {"exit", lenv_exit},
                             {"monitorStart", lenv_monitorStart},
                             {"monitorStop", lenv_monitorStop},
                             {"monitorWaitForCount", lenv_monitorWaitForCount},
                             {"luacacheOn", lenv_luacacheOn},
                             {"luacacheOff", lenv_luacacheOff},
                             {"luacacheAbandon", lenv_luacacheAbandon},
                             {NULL, NULL}};

    luaL_newlib(L, lualib_env);
    return 1;
}
