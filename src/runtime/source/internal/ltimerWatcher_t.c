

#include "internal/ltimerWatcher_t.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "service_t.h"

static int32_t ltimerWatcher_stop(lua_State* L)
{
    ltimerWatcher_tt* pTimerWatcherL = (ltimerWatcher_tt*)luaL_checkudata(L, 1, "timerWatcher");
    luaL_argcheck(L, pTimerWatcherL != NULL, 1, "invalid user data");
    if (pTimerWatcherL->pHandle) {
        timerWatcher_stop(pTimerWatcherL->pHandle);
        timerWatcher_release(pTimerWatcherL->pHandle);
        pTimerWatcherL->pHandle = NULL;
        lua_pushboolean(L, 1);
    }
    else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int32_t ltimerWatcher_gc(lua_State* L)
{
    ltimerWatcher_tt* pTimerWatcherL = (ltimerWatcher_tt*)luaL_checkudata(L, 1, "timerWatcher");
    luaL_argcheck(L, pTimerWatcherL != NULL, 1, "invalid user data");
    if (pTimerWatcherL->pHandle) {
        timerWatcher_stop(pTimerWatcherL->pHandle);
        timerWatcher_release(pTimerWatcherL->pHandle);
        pTimerWatcherL->pHandle = NULL;
    }
    return 0;
}

int32_t registerTimerWatcherL(lua_State* L)
{
    luaL_newmetatable(L, "timerWatcher");
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    struct luaL_Reg lua_timerWatcherFuncs[] = {{"stop", ltimerWatcher_stop},
                                               {"__close", ltimerWatcher_gc},
                                               {"__gc", ltimerWatcher_gc},
                                               {NULL, NULL}};

    luaL_setfuncs(L, lua_timerWatcherFuncs, 0);
    return 1;
};
