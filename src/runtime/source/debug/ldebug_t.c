

#include "debug/ldebug_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

// modify from skynet https://github.com/cloudwu/skynet/ lua-debugchannel

static const int32_t s_iHookKey = 0;

static lua_State* getLuaThread(lua_State* L, int32_t* pArg)
{
    if (lua_isthread(L, 1)) {
        *pArg = 1;
        return lua_tothread(L, 1);
    }
    else {
        *pArg = 0;
        return L;
    }
}

static void hook_f(lua_State* L, lua_Debug* pDebug)
{
    static const char* const s_szHookNames[] = {"call", "return", "line", "count", "tail call"};
    lua_rawgetp(L, LUA_REGISTRYINDEX, &s_iHookKey);
    lua_pushthread(L);

    if (lua_rawget(L, -2) == LUA_TFUNCTION) {
        lua_pushstring(L, s_szHookNames[(int32_t)pDebug->event]);
        if (pDebug->currentline >= 0) {
            lua_pushinteger(L, pDebug->currentline);
        }
        else {
            lua_pushnil(L);
        }
        lua_call(L, 2, 1);
        int32_t iYield = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (iYield) {
            lua_yield(L, 0);
        }
    }
}

static int32_t makeMask(const char* szMask, int32_t iCount)
{
    int32_t iMask = 0;
    if (strchr(szMask, 'c')) {
        iMask |= LUA_MASKCALL;
    }

    if (strchr(szMask, 'r')) {
        iMask |= LUA_MASKRET;
    }

    if (strchr(szMask, 'l')) {
        iMask |= LUA_MASKLINE;
    }

    if (iCount > 0) {
        iMask |= LUA_MASKCOUNT;
    }

    return iMask;
}

static int32_t ldeubg_sethook(lua_State* L)
{
    int32_t iArg   = 0;
    int32_t iMask  = 0;
    int32_t iCount = 0;

    lua_Hook   func      = NULL;
    lua_State* pLuaState = getLuaThread(L, &iArg);
    if (lua_isnoneornil(L, iArg + 1)) {
        lua_settop(L, iArg + 1);
    }
    else {
        const char* szMask = luaL_checkstring(L, iArg + 2);
        luaL_checktype(L, iArg + 1, LUA_TFUNCTION);
        iCount = (int32_t)luaL_optinteger(L, iArg + 3, 0);
        func   = hook_f;
        iMask  = makeMask(szMask, iCount);
    }

    if (lua_rawgetp(L, LUA_REGISTRYINDEX, &s_iHookKey) == LUA_TNIL) {
        lua_createtable(L, 0, 2);
        lua_pushvalue(L, -1);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &s_iHookKey);
        lua_pushstring(L, "k");
        lua_setfield(L, -2, "__mode");
        lua_pushvalue(L, -1);
        lua_setmetatable(L, -2);
    }

    lua_pushthread(pLuaState);
    lua_xmove(pLuaState, L, 1);
    lua_pushvalue(L, iArg + 1);
    lua_rawset(L, -3);
    lua_sethook(pLuaState, func, iMask, iCount);
    return 0;
}

int32_t luaopen_lruntime_debug(lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_debug[] = {{"sethook", ldeubg_sethook}, {NULL, NULL}};

    luaL_newlib(L, lualib_debug);
    return 1;
}
