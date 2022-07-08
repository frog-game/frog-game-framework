

#include "internal/lconnector_t.h"

#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "service_t.h"

static int32_t lconnector_close(lua_State* L)
{
    lconnector_tt* pConnector = (lconnector_tt*)luaL_checkudata(L, 1, "connector");
    luaL_argcheck(L, pConnector != NULL, 1, "invalid user data");
    if (pConnector->pHandle) {
        connector_close(pConnector->pHandle);
        connector_release(pConnector->pHandle);
        pConnector->pHandle = NULL;
    }
    return 0;
}

static int32_t lconnector_gc(lua_State* L)
{
    lconnector_tt* pConnector = (lconnector_tt*)luaL_checkudata(L, 1, "connector");
    luaL_argcheck(L, pConnector != NULL, 1, "invalid user data");

    if (pConnector->pHandle) {
        connector_release(pConnector->pHandle);
        pConnector->pHandle = NULL;
    }
    return 0;
}

int32_t registerConnectorL(lua_State* L)
{
    luaL_newmetatable(L, "connector");
    /* metatable.__index = metatable */
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    struct luaL_Reg lua_connectorFuncs[] = {{"close", lconnector_close},
                                            {"__close", lconnector_gc},
                                            {"__gc", lconnector_gc},
                                            {NULL, NULL}};

    luaL_setfuncs(L, lua_connectorFuncs, 0);
    return 1;
};
