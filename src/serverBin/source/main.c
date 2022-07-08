
#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

static inline void setPackage_cpath(lua_State* L, const char* szPackagePath)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "cpath");
    const char* cur_path = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_pushfstring(L, "%s;%s", cur_path, szPackagePath);
    lua_setfield(L, -2, "cpath");
    lua_pop(L, 1);
}

int32_t main(int32_t argc, char** argv)
{
    const char* szStartUpFileName = NULL;
    if (argc > 1) {
        szStartUpFileName = argv[1];
    }
    else {
        fprintf(stderr, "need a start-up file\n");
        return 1;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
#if defined(_WINDOWS) || defined(_WIN32)
    setPackage_cpath(L, "modules/?.dll");
#elif defined(__APPLE__)
    setPackage_cpath(L, "modules/?.dylib");
#elif defined(__linux__)
    setPackage_cpath(L, "modules/?.so");
#endif

    int32_t status = luaL_loadfile(L, szStartUpFileName);
    if (status != LUA_OK) {
        fprintf(stderr, "luaL_loadfile error:%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "lua_pcall error:%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    lua_close(L);
    return 0;
}