

#include "internal/ldnsResolve_t.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "service_t.h"

static int32_t ldnsResolve_parser(lua_State* L)
{
    ldnsResolve_tt* pDnsResolve = (ldnsResolve_tt*)luaL_checkudata(L, 1, "dnsResolve");
    luaL_argcheck(L, pDnsResolve != NULL, 1, "invalid user data");

    if (pDnsResolve->pHandle) {
        const char* pBuffer = NULL;
        size_t      nLength = 0;
        int32_t     iType   = lua_type(L, 2);
        if (iType == LUA_TSTRING) {
            pBuffer = lua_tolstring(L, 2, &nLength);
        }
        else {
            if (iType != LUA_TUSERDATA && iType != LUA_TLIGHTUSERDATA) {
                lua_pushinteger(L, 0);
                return 1;
            }
            pBuffer = (const char*)lua_touserdata(L, 2);
            nLength = luaL_checkinteger(L, 3);
        }

        int32_t iCount = dnsResolve_parser(pDnsResolve->pHandle, pBuffer, nLength);
        if (iCount > 0) {
            lua_createtable(L, iCount, 0);
            luaL_checkstack(L, iCount, NULL);
            for (int32_t i = 0; i < iCount; i++) {
                const char* szAddress = dnsResolve_getAddress(pDnsResolve->pHandle, i);
                lua_pushlstring(L, szAddress, strlen(szAddress));
                lua_rawseti(L, -2, i + 1);
            }
            return 1;
        }
    }
    return 0;
}

static int32_t ldnsResolve_gc(lua_State* L)
{
    ldnsResolve_tt* pDnsResolve = (ldnsResolve_tt*)luaL_checkudata(L, 1, "dnsResolve");
    luaL_argcheck(L, pDnsResolve != NULL, 1, "invalid user data");
    if (pDnsResolve->pHandle) {
        dnsResolve_close(pDnsResolve->pHandle);
        dnsResolve_release(pDnsResolve->pHandle);
        pDnsResolve->pHandle = NULL;
    }
    return 0;
}

int32_t registerDnsResolveL(lua_State* L)
{
    luaL_newmetatable(L, "dnsResolve");
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    struct luaL_Reg lua_dnsResolveFuncs[] = {{"parser", ldnsResolve_parser},
                                             {"close", ldnsResolve_gc},
                                             {"__close", ldnsResolve_gc},
                                             {"__gc", ldnsResolve_gc},
                                             {NULL, NULL}};

    luaL_setfuncs(L, lua_dnsResolveFuncs, 0);
    return 1;
};