

#include "stream/lwebSocket_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "codec/webSocket_t.h"
#include "serviceEvent_t.h"
#include "stream/webSocketStream_t.h"

static int32_t lhandle(struct lua_State* L)
{
    lua_pushlightuserdata(L, webSocketStreamCreate());
    return 1;
}

typedef struct lwebSocketFrame_s
{
    byteQueue_tt byteQueue;
    uint8_t      uiEvent;
} lwebSocketFrame_tt;

static int32_t lnew(struct lua_State* L)
{
    int64_t             sz     = lua_tointeger(L, 1);
    lwebSocketFrame_tt* pFrame = lua_newuserdatauv(L, sizeof(lwebSocketFrame_tt), 0);
    byteQueue_init(&pFrame->byteQueue, sz);
    pFrame->uiEvent = 0;
    return 1;
}

static inline void unMask(char* pBuffer, size_t nLength, char szMask[4], size_t nOffset)
{
    for (size_t i = 0; i < nLength; ++i) {
        pBuffer[i] ^= szMask[(i + nOffset) % 4];
    }
}

static int32_t lwrite(lua_State* L)
{
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    char*  pBuffer = NULL;
    size_t nLength = 0;

    if (lua_type(L, 2) == LUA_TSTRING) {
        pBuffer = (char*)lua_tolstring(L, 2, &nLength);
    }
    else {
        pBuffer = lua_touserdata(L, 2);
        nLength = (size_t)luaL_checkinteger(L, 3);
    }

    char*        pSwapBuffer = (char*)pBuffer;
    size_t       nSwapLength = nLength;
    byteQueue_tt byteQueue;
    byteQueue_init(&byteQueue, 0);
    byteQueue_swapBuffer(&byteQueue, &pSwapBuffer, &nSwapLength);

    char             szMask[4];
    webSocketHead_tt head;
    int32_t          r = webSocket_decodeHead(&byteQueue, &head);
    if (r == 1) {
        if (head.uiOffset > 4) {
            byteQueue_readOffset(&byteQueue, head.uiOffset - 4);
            byteQueue_readBytes(&byteQueue, szMask, 4, false);
        }
        else {
            byteQueue_readOffset(&byteQueue, head.uiOffset);
        }

        if ((head.uiFlag != 0) && (head.uiFlag != DEF_WS_FRAME_FINAL)) {
            pFrame->uiEvent = head.uiFlag | DEF_EVENT_MSG;
            byteQueue_reset(&pFrame->byteQueue);
        }

        if (head.uiPayloadLen > 0) {
            if (head.uiOffset > 4) {
                size_t nContiguousLength;
                char*  pContiguousBytesPointer =
                    byteQueue_peekContiguousBytesWrite(&pFrame->byteQueue, &nContiguousLength);
                byteQueue_write(&pFrame->byteQueue, pBuffer + head.uiOffset, head.uiPayloadLen);
                if (nContiguousLength >= head.uiPayloadLen) {
                    unMask(pContiguousBytesPointer, head.uiPayloadLen, szMask, 0);
                }
                else {
                    unMask(pContiguousBytesPointer, nContiguousLength, szMask, 0);
                    unMask(byteQueue_getBuffer(&pFrame->byteQueue),
                           head.uiPayloadLen - nContiguousLength,
                           szMask,
                           nContiguousLength);
                }
            }
            else {
                byteQueue_write(&pFrame->byteQueue, pBuffer + head.uiOffset, head.uiPayloadLen);
            }
        }

        if (head.uiFlag & DEF_WS_FRAME_FINAL) {
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
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
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
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");
    byteQueue_reset(&pFrame->byteQueue);
    return 0;
}

static int32_t lclear(lua_State* L)
{
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    byteQueue_clear(&pFrame->byteQueue);
    return 0;
}

static int32_t lsize(lua_State* L)
{
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");

    lua_pushinteger(L, byteQueue_getBytesReadable(&pFrame->byteQueue));
    return 1;
}

static int32_t levent(lua_State* L)
{
    lwebSocketFrame_tt* pFrame = lua_touserdata(L, 1);
    luaL_argcheck(L, pFrame != NULL, 1, "invalid user data");
    lua_pushinteger(L, pFrame->uiEvent);
    return 1;
}

int32_t luaopen_lruntime_webSocket(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_webSocketFuncs[] = {{"codecHandle", lhandle},
                                        {"new", lnew},
                                        {"write", lwrite},
                                        {"read", lread},
                                        {"reset", lreset},
                                        {"clear", lclear},
                                        {"size", lsize},
                                        {"event", levent},
                                        {NULL, NULL}};
    luaL_newlib(L, lualib_webSocketFuncs);
    return 1;
}
