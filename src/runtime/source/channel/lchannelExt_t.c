

#include "channel/lchannelExt_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "channel/channelCenter_t.h"
#include "channel/channel_t.h"

static int32_t lchannelExt_writePending(struct lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        lua_pushinteger(L, 0);
    }
    else {
        int32_t iWritePending = channel_getWritePending(pChannel);
        channel_release(pChannel);
        lua_pushinteger(L, iWritePending);
    }
    return 1;
}

static int32_t lchannelExt_writePendingBytes(struct lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        lua_pushinteger(L, 0);
    }
    else {
        size_t nWritePendingBytes = channel_getWritePendingBytes(pChannel);
        channel_release(pChannel);
        lua_pushinteger(L, nWritePendingBytes);
    }
    return 1;
}

static int32_t lchannelExt_receiveBufLength(struct lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        lua_pushinteger(L, 0);
    }
    else {
        size_t nReceiveBufLength = channel_getReceiveBufLength(pChannel);
        channel_release(pChannel);
        lua_pushinteger(L, nReceiveBufLength);
    }
    return 1;
}

static int32_t lchannelExt_addref(lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lchannelExt_release(lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }
    channel_release(pChannel);
    channel_release(pChannel);
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lchannelExt_localAddr(lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }

    char szAddressBuffer[64];

    inetAddress_tt address;
    if (!channel_getLocalAddr(pChannel, &address)) {
        channel_release(pChannel);
        return 0;
    }
    channel_release(pChannel);

    bool bIPPort = (bool)lua_toboolean(L, 2);
    if (bIPPort) {
        if (!inetAddress_toIPPortString(&address, szAddressBuffer, 64)) {
            return 0;
        }
    }
    else {
        if (!inetAddress_toIPString(&address, szAddressBuffer, 64)) {
            return 0;
        }
    }

    lua_pushstring(L, szAddressBuffer);
    return 1;
}

static int32_t lchannelExt_remoteAddr(lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }

    char szAddressBuffer[128];

    inetAddress_tt address;
    if (!channel_getRemoteAddr(pChannel, &address)) {
        channel_release(pChannel);
        return 0;
    }
    channel_release(pChannel);

    bool bIPPort = (bool)lua_toboolean(L, 2);
    if (bIPPort) {
        if (!inetAddress_toIPPortString(&address, szAddressBuffer, 128)) {
            return 0;
        }
    }
    else {
        if (!inetAddress_toIPString(&address, szAddressBuffer, 128)) {
            return 0;
        }
    }

    lua_pushstring(L, szAddressBuffer);
    return 1;
}

static int32_t lchannelExt_getAllChannels(struct lua_State* L)
{
    int32_t   iCount     = 0;
    uint32_t* pChanndels = channelCenter_getIds(&iCount);
    if (pChanndels == NULL) {
        return 0;
    }

    lua_createtable(L, iCount, 0);

    for (int64_t i = 0; i < iCount; ++i) {
        lua_pushinteger(L, pChanndels[i]);
        lua_rawseti(L, -2, i + 1);
    }
    mem_free(pChanndels);
    return 1;
}

int32_t luaopen_lruntime_channelExt(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif

    luaL_Reg lualib_channelExt[] = {{"addref", lchannelExt_addref},
                                    {"release", lchannelExt_release},
                                    {"localAddr", lchannelExt_localAddr},
                                    {"remoteAddr", lchannelExt_remoteAddr},
                                    {"writePending", lchannelExt_writePending},
                                    {"writePendingBytes", lchannelExt_writePendingBytes},
                                    {"receiveBufLength", lchannelExt_receiveBufLength},
                                    {"gets", lchannelExt_getAllChannels},
                                    {NULL, NULL}};
    luaL_newlib(L, lualib_channelExt);
    return 1;
}
