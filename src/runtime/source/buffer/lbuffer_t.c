

#include "buffer/lbuffer_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "byteQueue_t.h"

static int32_t lbuffer_new(lua_State* L)
{
    int64_t       sz         = lua_tointeger(L, 1);
    byteQueue_tt* pByteQueue = lua_newuserdatauv(L, sizeof(byteQueue_tt), 0);
    byteQueue_init(pByteQueue, sz);
    return 1;
}

static int32_t lbuffer_clear(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }
    byteQueue_clear(pByteQueue);
    return 0;
}

static int32_t lbuffer_reset(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }
    byteQueue_reset(pByteQueue);
    return 0;
}

static int32_t lbuffer_capacity(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }
    lua_pushinteger(L, byteQueue_getCapacity(pByteQueue));
    return 1;
}

static int32_t lbuffer_read(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }
    int64_t sz       = luaL_checkinteger(L, 2);
    size_t  nWritten = byteQueue_getBytesReadable(pByteQueue);
    if (sz > nWritten) {
        lua_pushnil(L);
        lua_pushinteger(L, nWritten);
    }
    else {
        size_t      nReadBytes = 0;
        const char* pBuffer    = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
        if (sz > nReadBytes) {
            luaL_Buffer b;
            luaL_buffinitsize(L, &b, nWritten);
            luaL_addlstring(&b, pBuffer, nReadBytes);
            luaL_addlstring(&b, byteQueue_getBuffer(pByteQueue), sz - nReadBytes);
            luaL_pushresult(&b);
            lua_pushinteger(L, nWritten - sz);
            byteQueue_readOffset(pByteQueue, sz);
        }
        else {
            lua_pushlstring(L, pBuffer, sz);
            lua_pushinteger(L, nWritten - sz);
            byteQueue_readOffset(pByteQueue, sz);
        }
    }
    return 2;
}

static int32_t lbuffer_readLineEOL(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);

    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }

    int64_t nOffset = luaL_checkinteger(L, 2);

    size_t nWritten = byteQueue_getBytesReadable(pByteQueue);
    if (nOffset >= nWritten) {
        lua_pushnil(L);
        lua_pushinteger(L, nWritten);
    }
    else {
        size_t      nReadBytes = 0;
        const char* pBuffer    = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
        if (nWritten != nReadBytes) {
            if (nOffset < nReadBytes) {
                const char* szEOL = memchr(pBuffer + nOffset, '\n', nReadBytes - nOffset);
                if (szEOL != NULL) {
                    size_t n = szEOL - pBuffer;
                    lua_pushlstring(L, pBuffer, n);
                    lua_pushinteger(L, 0);
                    byteQueue_readOffset(pByteQueue, n + 1);
                    return 2;
                }
            }

            const char* szEOL = memchr(
                byteQueue_getBuffer(pByteQueue) + (nOffset - nReadBytes), '\n', nWritten - nOffset);
            if (szEOL == NULL) {
                lua_pushnil(L);
                lua_pushinteger(L, nWritten);
                return 2;
            }
            size_t      n = szEOL - byteQueue_getBuffer(pByteQueue);
            luaL_Buffer b;
            luaL_buffinitsize(L, &b, nReadBytes + n);
            luaL_addlstring(&b, pBuffer, nReadBytes);
            luaL_addlstring(&b, byteQueue_getBuffer(pByteQueue), n);
            luaL_pushresult(&b);
            lua_pushinteger(L, 0);
            byteQueue_readOffset(pByteQueue, nReadBytes + n + 1);
        }
        else {
            const char* szEOL = memchr(pBuffer + nOffset, '\n', nWritten - nOffset);
            if (szEOL == NULL) {
                lua_pushnil(L);
                lua_pushinteger(L, nWritten);
                return 2;
            }
            size_t n = szEOL - pBuffer;
            lua_pushlstring(L, pBuffer, n);
            lua_pushinteger(L, 0);
            byteQueue_readOffset(pByteQueue, n + 1);
        }
    }
    return 2;
}

static int32_t lbuffer_readLineCRLF(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);

    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }

    int64_t nOffset = luaL_checkinteger(L, 2);

    size_t nWritten = byteQueue_getBytesReadable(pByteQueue);
    if (nOffset >= nWritten) {
        lua_pushnil(L);
        lua_pushinteger(L, nWritten);
    }
    else {
        size_t      nReadBytes = 0;
        const char* pBuffer    = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
        if (nWritten != nReadBytes) {
            if (nOffset < nReadBytes) {
                const char* szEOL = memchr(pBuffer + nOffset, '\n', nReadBytes - nOffset);
                if (szEOL != NULL) {
                    size_t n = szEOL - pBuffer;
                    if (n > 0 && *(szEOL - 1) == '\r') {
                        lua_pushlstring(L, pBuffer, n - 1);
                        lua_pushinteger(L, 0);
                        byteQueue_readOffset(pByteQueue, n + 1);
                        return 2;
                    }
                }
            }
            const char* pBuf  = byteQueue_getBuffer(pByteQueue);
            const char* szEOL = memchr(pBuf + (nOffset - nReadBytes), '\n', nWritten - nOffset);
            if (szEOL != NULL) {
                size_t n = szEOL - pBuf;
                if (n > 1) {
                    if (*(szEOL - 1) == '\r') {
                        luaL_Buffer b;
                        luaL_buffinitsize(L, &b, nReadBytes + n);
                        luaL_addlstring(&b, pBuffer, nReadBytes);
                        luaL_addlstring(&b, pBuf, n - 1);
                        luaL_pushresult(&b);
                        lua_pushinteger(L, 0);
                        byteQueue_readOffset(pByteQueue, nReadBytes + n + 1);
                        return 2;
                    }
                }
                else if (n == 1) {
                    if (*(szEOL - 1) == '\r') {
                        lua_pushlstring(L, pBuffer, nReadBytes);
                        lua_pushinteger(L, 0);
                        byteQueue_readOffset(pByteQueue, nReadBytes + n + 1);
                        return 2;
                    }
                }
                else {
                    if (*(pBuffer + nReadBytes - 1) == '\r') {
                        lua_pushlstring(L, pBuffer, nReadBytes - 1);
                        lua_pushinteger(L, 0);
                        byteQueue_readOffset(pByteQueue, nReadBytes + n + 1);
                        return 2;
                    }
                }
            }
        }
        else {
            const char* szEOL = memchr(pBuffer + nOffset, '\n', nWritten - nOffset);
            if (szEOL != NULL) {
                size_t n = szEOL - pBuffer;
                if (n > 0 && *(szEOL - 1) == '\r') {
                    lua_pushlstring(L, pBuffer, n - 1);
                    lua_pushinteger(L, 0);
                    byteQueue_readOffset(pByteQueue, n + 1);
                    return 2;
                }
            }
        }
    }

    lua_pushnil(L);
    lua_pushinteger(L, nWritten);
    return 2;
}

static int32_t lbuffer_readAll(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }

    if (byteQueue_empty(pByteQueue)) {
        return 0;
    }

    size_t nWritten = byteQueue_getBytesReadable(pByteQueue);

    size_t      nReadBytes = 0;
    const char* pBuffer    = byteQueue_peekContiguousBytesRead(pByteQueue, &nReadBytes);
    if (nWritten == nReadBytes) {
        lua_pushlstring(L, pBuffer, nReadBytes);
        byteQueue_readOffset(pByteQueue, nReadBytes);
    }
    else {
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        luaL_addlstring(&b, pBuffer, nReadBytes);
        luaL_addlstring(&b, byteQueue_getBuffer(pByteQueue), nWritten - nReadBytes);
        luaL_pushresult(&b);
        byteQueue_readOffset(pByteQueue, nWritten);
    }
    return 1;
}

static int32_t lbuffer_write(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }

    const char* pBuffer = NULL;
    size_t      nLength = 0;

    if (lua_type(L, 2) == LUA_TSTRING) {
        pBuffer = lua_tolstring(L, 2, &nLength);
    }
    else {
        pBuffer = (const char*)lua_touserdata(L, 2);
        nLength = (size_t)luaL_checkinteger(L, 3);
    }

    byteQueue_write(pByteQueue, pBuffer, nLength);
    lua_pushinteger(L, byteQueue_getBytesReadable(pByteQueue));
    return 1;
}

static int32_t lbuffer_size(lua_State* L)
{
    byteQueue_tt* pByteQueue = lua_touserdata(L, 1);
    if (pByteQueue == NULL) {
        return luaL_error(L, "lbuffer nil");
    }

    lua_pushinteger(L, byteQueue_getBytesReadable(pByteQueue));
    return 1;
}

int32_t luaopen_lruntime_buffer(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_buffer[] = {{"new", lbuffer_new},
                                {"clear", lbuffer_clear},
                                {"reset", lbuffer_reset},
                                {"capacity", lbuffer_capacity},
                                {"read", lbuffer_read},
                                {"readLineEOL", lbuffer_readLineEOL},
                                {"readLineCRLF", lbuffer_readLineCRLF},
                                {"readAll", lbuffer_readAll},
                                {"write", lbuffer_write},
                                {"size", lbuffer_size},
                                {NULL, NULL}};
    luaL_newlib(L, lualib_buffer);
    return 1;
}
