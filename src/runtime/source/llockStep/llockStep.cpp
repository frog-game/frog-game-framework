

#include "llockStep.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "llockStepPack.h"
#include "llockStepPackStream.h"
#include "log_t.h"
#include "serviceEvent_t.h"
#include "utility_t.h"
}

#include "lua.hpp"

static int32_t lhandle(struct lua_State* L)
{
    lua_pushlightuserdata(L, lockStepPackStreamCreate());
    return 1;
}

typedef struct llockStepPackFrame_s
{
    byteQueue_tt byteQueue;
    uint32_t     uiToken;
    uint8_t      uiEvent;
} llockStepPackFrame_tt;

static int32_t lnew(struct lua_State* L)
{
    int64_t                sz = lua_tointeger(L, 1);
    llockStepPackFrame_tt* pFrame =
        (llockStepPackFrame_tt*)lua_newuserdatauv(L, sizeof(llockStepPackFrame_tt), 0);
    byteQueue_init(&pFrame->byteQueue, sz);
    pFrame->uiToken = 0;
    pFrame->uiEvent = 0;
    return 1;
}

static int32_t lwrite(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    char*  pBuffer = NULL;
    size_t nLength = 0;

    if (lua_type(L, 2) == LUA_TSTRING) {
        pBuffer = (char*)lua_tolstring(L, 2, &nLength);
    }
    else {
        pBuffer = (char*)lua_touserdata(L, 2);
        nLength = (size_t)luaL_checkinteger(L, 3);
    }

    char*        pSwapBuffer = (char*)pBuffer;
    size_t       nSwapLength = nLength;
    byteQueue_tt byteQueue;
    byteQueue_init(&byteQueue, 0);
    byteQueue_swapBuffer(&byteQueue, &pSwapBuffer, &nSwapLength);

    uint8_t             szToken[4];
    lockStepPackHead_tt head;
    int32_t             r = lockStepPack_decodeHead(&byteQueue, &head);
    if (r == 1) {
        if (head.uiOffset > 4) {
            byteQueue_readOffset(&byteQueue, head.uiOffset - 4);
            byteQueue_readBytes(&byteQueue, szToken, 4, false);
            pFrame->uiToken = ((uint32_t)szToken[0] << 24) | ((uint32_t)szToken[1] << 16) |
                              ((uint32_t)szToken[2] << 8) | szToken[3];
        }
        else {
            byteQueue_readOffset(&byteQueue, head.uiOffset);
            pFrame->uiToken = 0;
        }

        if ((head.uiFlag != 0) && (head.uiFlag != DEF_TP_FRAME_FINAL)) {
            pFrame->uiEvent = head.uiFlag | DEF_EVENT_MSG;
            byteQueue_reset(&pFrame->byteQueue);
        }

        if (head.uiPayloadLen > 0) {
            byteQueue_write(&pFrame->byteQueue, pBuffer + head.uiOffset, head.uiPayloadLen);
        }

        if (head.uiFlag & DEF_TP_FRAME_FINAL) {
            lua_pushinteger(L, 1);
            lua_pushinteger(L, head.uiPayloadLen);
            return 2;
        }
        else {
            lua_pushinteger(L, 2);
            lua_pushinteger(L, head.uiPayloadLen);
            return 2;
        }
    }
    else {
        lua_pushinteger(L, 0);
        return 1;
    }
}

static int32_t lread(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    if (byteQueue_empty(&pFrame->byteQueue)) {
        return 0;
    }

    size_t nWritten = byteQueue_getBytesReadable(&pFrame->byteQueue);

    size_t      nReadBytes = 0;
    const char* pBuffer    = byteQueue_peekContiguousBytesRead(&pFrame->byteQueue, &nReadBytes);
    if (nWritten > nReadBytes) {
        luaL_Buffer b;
        luaL_buffinitsize(L, &b, nWritten);
        luaL_addlstring(&b, pBuffer, nReadBytes);
        luaL_addlstring(&b, byteQueue_getBuffer(&pFrame->byteQueue), nWritten - nReadBytes);
        luaL_pushresult(&b);
        lua_pushinteger(L, nWritten);
        byteQueue_readOffset(&pFrame->byteQueue, nWritten);
    }
    else {
        lua_pushlstring(L, pBuffer, nReadBytes);
        lua_pushinteger(L, nReadBytes);
        byteQueue_readOffset(&pFrame->byteQueue, nReadBytes);
    }
    return 2;
}

static int32_t lreset(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    byteQueue_reset(&pFrame->byteQueue);
    return 0;
}

static int32_t lclear(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    byteQueue_clear(&pFrame->byteQueue);
    return 0;
}

static int32_t lsize(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    lua_pushinteger(L, byteQueue_getBytesReadable(&pFrame->byteQueue));
    return 1;
}

static int32_t ltoken(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    lua_pushinteger(L, pFrame->uiToken);
    return 1;
}

static int32_t levent(lua_State* L)
{
    llockStepPackFrame_tt* pFrame = (llockStepPackFrame_tt*)lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    lua_pushinteger(L, pFrame->uiEvent);
    return 1;
}

int32_t luaopen_lruntime_llockStep(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_lockStepPackFuncs[] = {{"codecHandle", lhandle},
                                           {"new", lnew},
                                           {"write", lwrite},
                                           {"read", lread},
                                           {"reset", lreset},
                                           {"clear", lclear},
                                           {"size", lsize},
                                           {"token", ltoken},
                                           {"event", levent},
                                           {NULL, NULL}};
    luaL_newlib(L, lualib_lockStepPackFuncs);
    return 1;
}
