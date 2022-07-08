

#include "internal/lpackagePath_t.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

void setPackage_path(lua_State* L, const char* szPackagePath)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char* cur_path = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushfstring(L, "%s;%s/?.lua", cur_path, szPackagePath);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

void setPackage_cpath(lua_State* L, const char* szPackagePath)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");
    const char* cur_path = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushfstring(L, "%s;%s", cur_path, szPackagePath);
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);
}