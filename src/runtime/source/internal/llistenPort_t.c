

#include "internal/llistenPort_t.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "service_t.h"

static int32_t llistenPort_gc(lua_State* L)
{
    llistenPort_tt* pListenPort = (llistenPort_tt*)luaL_checkudata(L, 1, "listenPort");
    luaL_argcheck(L, pListenPort != NULL, 1, "invalid user data");
    if (pListenPort->pHandle) {
        listenPort_close(pListenPort->pHandle);
        listenPort_release(pListenPort->pHandle);
        pListenPort->pHandle = NULL;
    }
    return 0;
}

int32_t registerListenPortL(lua_State* L)
{
    luaL_newmetatable(L, "listenPort");
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    struct luaL_Reg lua_listenPortFuncs[] = {{"close", llistenPort_gc},
                                             {"__close", llistenPort_gc},
                                             {"__gc", llistenPort_gc},
                                             {NULL, NULL}};

    luaL_setfuncs(L, lua_listenPortFuncs, 0);
    return 1;
};
