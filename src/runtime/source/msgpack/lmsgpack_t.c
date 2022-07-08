

#include "msgpack/lmsgpack_t.h"

#include <stdbool.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "msgpack/msgpackDecode_t.h"
#include "msgpack/msgpackEncode_t.h"

#define def_MaxDepth 32

static inline void packValue(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                             int32_t iDepth);

static inline void packTableMetapairs(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                                      int32_t iDepth)
{
    msgpackEncode_writeMap(pMsgBuffer, 0);
    lua_pushvalue(L, iIndex);
    lua_call(L, 1, 3);
    for (;;) {
        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_copy(L, -5, -3);
        lua_call(L, 2, 2);
        int32_t iType = lua_type(L, -2);
        if (iType == LUA_TNIL) {
            lua_pop(L, 4);
            break;
        }
        packValue(L, pMsgBuffer, -2, iDepth);
        packValue(L, pMsgBuffer, -1, iDepth);
        lua_pop(L, 1);
    }
    msgpackEncode_writeNil(pMsgBuffer);
}

static inline int32_t packTableArray(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                                     int32_t iDepth)
{
    int32_t iSize = lua_rawlen(L, iIndex);
    msgpackEncode_writeMap(pMsgBuffer, iSize);

    for (int32_t i = 1; i <= iSize; ++i) {
        lua_rawgeti(L, iIndex, i);
        packValue(L, pMsgBuffer, -1, iDepth);
        lua_pop(L, 1);
    }
    return iSize;
}

static inline void packTableHash(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                                 int32_t iDepth, int32_t iSize)
{
    lua_pushnil(L);
    while (lua_next(L, iIndex) != 0) {
        if (lua_type(L, -2) == LUA_TNUMBER) {
            if (lua_isinteger(L, -2)) {
                lua_Integer x = lua_tointeger(L, -2);
                if (x > 0 && x <= iSize) {
                    lua_pop(L, 1);
                    continue;
                }
            }
        }
        packValue(L, pMsgBuffer, -2, iDepth);
        packValue(L, pMsgBuffer, -1, iDepth);
        lua_pop(L, 1);
    }
    msgpackEncode_writeNil(pMsgBuffer);
}

static inline void packTable(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                             int32_t iDepth)
{
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    if (iIndex < 0) {
        iIndex = lua_gettop(L) + iIndex + 1;
    }

    if (luaL_getmetafield(L, iIndex, "__pairs") != LUA_TNIL) {
        packTableMetapairs(L, pMsgBuffer, iIndex, iDepth);
    }
    else {
        int32_t iSize = packTableArray(L, pMsgBuffer, iIndex, iDepth);
        packTableHash(L, pMsgBuffer, iIndex, iDepth, iSize);
    }
}

static inline void packValue(lua_State* L, msgpackEncode_tt* pMsgBuffer, int32_t iIndex,
                             int32_t iDepth)
{
    if (iDepth > def_MaxDepth) {
        luaL_error(L, "can't pack > max depth table");
    }

    int32_t iType = lua_type(L, iIndex);

    switch (iType) {
    case LUA_TNIL:
    {
        msgpackEncode_writeNil(pMsgBuffer);
    } break;
    case LUA_TNUMBER:
    {
        if (lua_isinteger(L, iIndex)) {
            msgpackEncode_writeInteger(pMsgBuffer, lua_tointeger(L, iIndex));
        }
        else {
            msgpackEncode_writeReal(pMsgBuffer, lua_tonumber(L, iIndex));
        }
    } break;
    case LUA_TBOOLEAN:
    {
        msgpackEncode_writeBoolean(pMsgBuffer, lua_toboolean(L, iIndex));
    } break;
    case LUA_TSTRING:
    {
        size_t      sz  = 0;
        const char* str = lua_tolstring(L, iIndex, &sz);
        msgpackEncode_writeString(pMsgBuffer, str, sz);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        msgpackEncode_writeUserPointer(pMsgBuffer, lua_touserdata(L, iIndex));
    } break;
    case LUA_TTABLE:
    {
        if (iIndex < 0) {
            iIndex = lua_gettop(L) + iIndex + 1;
        }
        packTable(L, pMsgBuffer, iIndex, iDepth + 1);
    } break;
    default:
    {
        luaL_error(L, "can't pack unsupport type %s to", lua_typename(L, iType));
    }
    }
}

static void* msgpackEncode_expand(void* pBuffer, size_t nUseLength, size_t nLength, void* pData)
{
    lua_State* L = (lua_State*)pData;
    if (nLength > 0) {
        void* pNewBuffer = lua_newuserdatauv(L, nLength, 0);
        if (nUseLength > 0) {
            memcpy(pNewBuffer, pBuffer, nUseLength);
        }
        lua_replace(L, lua_upvalueindex(1));
        lua_pushinteger(L, nLength);
        lua_replace(L, lua_upvalueindex(2));
        return pNewBuffer;
    }
    else {
        lua_pushnil(L);
        lua_replace(L, lua_upvalueindex(1));
        lua_pushinteger(L, 0);
        lua_replace(L, lua_upvalueindex(2));
    }
    return NULL;
}

static int32_t lencode(lua_State* L)
{
    int32_t iCount = lua_gettop(L);
    if (iCount == 0) {
        return 0;
    }

    void*  pBuffer = lua_touserdata(L, lua_upvalueindex(1));
    size_t nLength = lua_tointeger(L, lua_upvalueindex(2));

    msgpackEncode_tt msgBuffer;
    msgpackEncode_init(&msgBuffer, pBuffer, nLength, msgpackEncode_expand, L);

    for (int32_t i = 1; i <= iCount; ++i) {
        packValue(L, &msgBuffer, i, 0);
    }

    size_t nOffset = 0;
    msgpackEncode_swap(&msgBuffer, &pBuffer, &nLength, &nOffset);
    lua_pushlightuserdata(L, pBuffer);
    lua_pushinteger(L, nOffset);
    return 2;
}

static int32_t lencodeString(lua_State* L)
{
    int32_t iCount = lua_gettop(L);
    if (iCount == 0) {
        return 0;
    }

    void*  pBuffer = lua_touserdata(L, lua_upvalueindex(1));
    size_t nLength = lua_tointeger(L, lua_upvalueindex(2));

    msgpackEncode_tt msgBuffer;
    msgpackEncode_init(&msgBuffer, pBuffer, nLength, msgpackEncode_expand, L);

    for (int32_t i = 1; i <= iCount; ++i) {
        packValue(L, &msgBuffer, i, 0);
    }

    size_t nOffset = 0;
    msgpackEncode_swap(&msgBuffer, &pBuffer, &nLength, &nOffset);
    lua_pushlstring(L, (char*)pBuffer, nOffset);
    return 1;
}

static inline bool unpackValue(lua_State* L, msgpackDecode_tt* pMsgBuffer);

static inline bool unpackTable(lua_State* L, int64_t iArrayCount, msgpackDecode_tt* pMsgBuffer)
{
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    lua_createtable(L, iArrayCount, 0);

    for (int64_t i = 1; i <= iArrayCount; ++i) {
        if (!unpackValue(L, pMsgBuffer)) {
            return false;
        }
        lua_rawseti(L, -2, i);
    }

    for (;;) {
        if (!unpackValue(L, pMsgBuffer)) {
            return false;
        }

        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return true;
        }

        if (!unpackValue(L, pMsgBuffer)) {
            return false;
        }
        lua_rawset(L, -3);
    }
    return true;
}

static inline bool unpackValue(lua_State* L, msgpackDecode_tt* pMsgBuffer)
{
    enMsgpackValueType eType = msgpackDecode_getType(pMsgBuffer);
    switch (eType) {
    case eMsgpackInteger:
    {
        int64_t iValue = 0;
        if (!msgpackDecode_readInteger(pMsgBuffer, &iValue)) {
            return false;
        }
        lua_pushinteger(L, iValue);
    } break;
    case eMsgpackBoolean:
    {
        bool bValue = false;
        if (!msgpackDecode_readBoolean(pMsgBuffer, &bValue)) {
            return false;
        }
        lua_pushboolean(L, bValue);
    } break;
    case eMsgpackReal:
    {
        double fValue = 0.0;
        if (!msgpackDecode_readReal(pMsgBuffer, &fValue)) {
            return false;
        }
        lua_pushnumber(L, fValue);
    } break;
    case eMsgpackString:
    {
        int64_t     iLength = 0;
        const char* szStr   = msgpackDecode_readString(pMsgBuffer, &iLength);
        if (iLength == -1) {
            return false;
        }
        else {
            lua_pushlstring(L, szStr, iLength);
        }
    } break;
    case eMsgpackBinary:
    {
        int64_t     iLength = 0;
        const char* szStr   = msgpackDecode_readBinary(pMsgBuffer, &iLength);
        if (iLength == -1) {
            return false;
        }
        else {
            lua_pushlstring(L, szStr, iLength);
        }
    } break;
    case eMsgpackMap:
    {
        int64_t iCount = 0;
        if (!msgpackDecode_readMap(pMsgBuffer, &iCount)) {
            return false;
        }
        unpackTable(L, iCount, pMsgBuffer);
    } break;
    case eMsgpackNil:
    {
        if (!msgpackDecode_skipNil(pMsgBuffer)) {
            return false;
        }
        lua_pushnil(L);
    } break;
    case eMsgpackUserPointer:
    {
        void* pValue = NULL;
        if (!msgpackDecode_readUserPointer(pMsgBuffer, &pValue)) {
            return false;
        }
        lua_pushlightuserdata(L, pValue);
    } break;
    default:
    {
        return false;
    } break;
    }
    return true;
}

static int32_t ldecode(lua_State* L)
{
    const void* pBuffer = NULL;
    size_t      nLength = 0;

    int32_t iType = lua_type(L, 1);
    if (iType == LUA_TSTRING) {
        pBuffer = lua_tolstring(L, 1, &nLength);
    }
    else {
        if (iType != LUA_TUSERDATA && iType != LUA_TLIGHTUSERDATA) {
            return 0;
        }
        pBuffer = (const char*)lua_touserdata(L, 1);
        nLength = (size_t)luaL_checkinteger(L, 2);
    }

    if (nLength == 0 || pBuffer == NULL) {
        return 0;
    }

    msgpackDecode_tt msgBuffer;
    msgpackDecode_init(&msgBuffer, pBuffer, nLength, 0);

    lua_settop(L, 1);

    for (int32_t i = 0;; ++i) {
        if (i % 8 == 7) {
            luaL_checkstack(L, LUA_MINSTACK, NULL);
        }
        if (msgpackDecode_isEnd(&msgBuffer)) {
            break;
        }
        if (!unpackValue(L, &msgBuffer)) {
            return 0;
        }
    }
    return lua_gettop(L) - 1;
}

int32_t luaopen_lruntime_msgpack(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif

    luaL_Reg lualib_msgpack[] = {{"decode", ldecode}, {NULL, NULL}};

    luaL_Reg lualib_msgpack_buffer[] = {
        {"encode", lencode}, {"encodeString", lencodeString}, {NULL, NULL}};

    lua_createtable(L,
                    0,
                    sizeof(lualib_msgpack) / sizeof(lualib_msgpack[0]) +
                        sizeof(lualib_msgpack_buffer) / sizeof(lualib_msgpack_buffer[0]) - 2);
    lua_newuserdatauv(L, 2048, 0);
    lua_pushinteger(L, 2048);
    luaL_setfuncs(L, lualib_msgpack_buffer, 2);
    luaL_setfuncs(L, lualib_msgpack, 0);
    return 1;
}
