

#include "db/lredis_t.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "log_t.h"
#include "utility_t.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "redis_parser.h"

typedef enum
{
    redis_reply_type_line,
    redis_reply_type_error,
    redis_reply_type_integer,
    redis_reply_type_bulk,
    redis_reply_type_array,
} redis_reply_type_tt;

struct redisReply_s;
typedef struct redisReply_s redisReply_tt;

typedef struct replyString_s
{
    char*   value;
    int32_t length;
} replyString_tt;

struct redisReply_s
{
    redis_reply_type_tt type;
    redisReply_tt*      next;
    bool                bEof;
    char                value[];
};

static inline redisReply_tt* redisReply_new(redis_reply_type_tt type)
{
    redisReply_tt* p = NULL;
    switch (type) {
    case redis_reply_type_line:
    case redis_reply_type_error:
    case redis_reply_type_bulk:
    {
        p                      = mem_malloc(sizeof(redisReply_tt) + sizeof(replyString_tt));
        p->type                = type;
        p->next                = NULL;
        p->bEof                = false;
        replyString_tt* pValue = (replyString_tt*)p->value;
        pValue->value          = NULL;
        pValue->length         = 0;
    } break;
    case redis_reply_type_integer:
    {
        p                     = mem_malloc(sizeof(redisReply_tt) + sizeof(int64_t));
        p->type               = redis_reply_type_integer;
        p->next               = NULL;
        p->bEof               = false;
        *((int64_t*)p->value) = 0;
    } break;
    case redis_reply_type_array:
    {
        p                     = mem_malloc(sizeof(redisReply_tt) + sizeof(int64_t));
        p->type               = redis_reply_type_array;
        p->next               = NULL;
        p->bEof               = false;
        *((int64_t*)p->value) = 0;
    } break;
    }
    return p;
}

static inline void redisReply_free(redisReply_tt* p)
{
    if (p->next) {
        redisReply_free(p->next);
        p->next = NULL;
    }

    switch (p->type) {
    case redis_reply_type_line:
    case redis_reply_type_error:
    case redis_reply_type_bulk:
    {
        replyString_tt* pValue = (replyString_tt*)p->value;
        mem_free(pValue->value);
        mem_free(p);
    } break;
    case redis_reply_type_integer:
    case redis_reply_type_array:
    {
        mem_free(p);
    } break;
    }
}

typedef struct lredisParser_s
{
    redis_parser_tt parser;
    int32_t         iCount;
    size_t          nCapacity;
    size_t          nLength;
    int32_t         iBulkLength;
    redisReply_tt*  pReply;
    redisReply_tt*  pLastReply;
} lredisParser_tt;

static int32_t redis_complete(redis_parser_tt* p)
{
    lredisParser_tt* pParser = p->userData;
    assert(pParser->pLastReply);
    pParser->pLastReply->bEof = true;
    ++pParser->iCount;
    return 0;
}

static int32_t redis_line(redis_parser_tt* p, const char* data, int32_t length)
{
    lredisParser_tt* pParser = p->userData;

    redisReply_tt*  pReply      = redisReply_new(redis_reply_type_line);
    replyString_tt* replyString = (replyString_tt*)pReply->value;
    replyString->length         = length;
    if (replyString->length > 0) {
        replyString->value = mem_malloc(length);
        memcpy(replyString->value, data, length);
    }

    if (pParser->pReply) {
        pParser->pLastReply->next = pReply;
    }
    else {
        pParser->pReply = pReply;
    }
    pParser->pLastReply = pReply;
    return 0;
}

static int32_t redis_error(redis_parser_tt* p, const char* data, int32_t length)
{
    lredisParser_tt* pParser = p->userData;

    redisReply_tt*  pReply      = redisReply_new(redis_reply_type_error);
    replyString_tt* replyString = (replyString_tt*)pReply->value;
    replyString->length         = length;
    if (replyString->length > 0) {
        replyString->value = mem_malloc(length);
        memcpy(replyString->value, data, length);
    }
    if (pParser->pReply) {
        pParser->pLastReply->next = pReply;
    }
    else {
        pParser->pReply = pReply;
    }
    pParser->pLastReply = pReply;
    return 0;
}

static int32_t redis_integer(redis_parser_tt* p, int64_t data)
{
    lredisParser_tt* pParser = p->userData;

    redisReply_tt* pReply      = redisReply_new(redis_reply_type_integer);
    *((int64_t*)pReply->value) = data;
    if (pParser->pReply) {
        pParser->pLastReply->next = pReply;
    }
    else {
        pParser->pReply = pReply;
    }
    pParser->pLastReply = pReply;
    return 0;
}

static int32_t redis_bulk_length(redis_parser_tt* p, int32_t length)
{
    lredisParser_tt* pParser = p->userData;

    redisReply_tt*  pReply      = redisReply_new(redis_reply_type_bulk);
    replyString_tt* replyString = (replyString_tt*)pReply->value;
    pParser->iBulkLength        = length;
    if (length > 0) {
        replyString->value = mem_malloc(length);
    }
    if (pParser->pReply) {
        pParser->pLastReply->next = pReply;
    }
    else {
        pParser->pReply = pReply;
    }
    pParser->pLastReply = pReply;
    return 0;
}

static int32_t redis_bulk(redis_parser_tt* p, const char* data, int32_t length)
{
    lredisParser_tt* pParser = p->userData;
    assert(pParser->pLastReply && (pParser->pLastReply->type == redis_reply_type_bulk));
    replyString_tt* replyString = (replyString_tt*)pParser->pLastReply->value;
    if (replyString->length + length > pParser->iBulkLength) {
        return -1;
    }

    memcpy(replyString->value + replyString->length, data, length);
    replyString->length += length;
    return 0;
}

static int32_t redis_array_count(redis_parser_tt* p, int64_t data)
{
    lredisParser_tt* pParser = p->userData;

    redisReply_tt* pReply      = redisReply_new(redis_reply_type_array);
    *((int64_t*)pReply->value) = data;
    if (pParser->pReply) {
        pParser->pLastReply->next = pReply;
    }
    else {
        pParser->pReply = pReply;
    }
    pParser->pLastReply = pReply;
    return 0;
}

static redis_parser_settings_tt s_redisParserSettings = {.on_complete    = redis_complete,
                                                         .on_line        = redis_line,
                                                         .on_error       = redis_error,
                                                         .on_integer     = redis_integer,
                                                         .on_bulk_length = redis_bulk_length,
                                                         .on_bulk        = redis_bulk,
                                                         .on_array_count = redis_array_count};

static int32_t lnew(struct lua_State* L)
{
    lredisParser_tt* pRedisParser = lua_newuserdatauv(L, sizeof(lredisParser_tt), 1);
    redis_parser_init(&pRedisParser->parser, pRedisParser);
    pRedisParser->nCapacity   = 2048;
    pRedisParser->nLength     = 0;
    pRedisParser->iBulkLength = 0;

    pRedisParser->iCount     = 0;
    pRedisParser->pReply     = NULL;
    pRedisParser->pLastReply = NULL;
    lua_newuserdatauv(L, pRedisParser->nCapacity, 0);
    lua_setiuservalue(L, 1, 1);
    return 1;
}

static int32_t lreset(struct lua_State* L)
{
    lredisParser_tt* pRedisParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pRedisParser != NULL, 1, "invalid user data");
    redis_parser_reset(&pRedisParser->parser);
    pRedisParser->nLength     = 0;
    pRedisParser->iBulkLength = 0;
    pRedisParser->iCount      = 0;
    pRedisParser->pLastReply  = NULL;
    if (pRedisParser->pReply) {
        redisReply_free(pRedisParser->pReply);
        pRedisParser->pReply = NULL;
    }
    if (pRedisParser->nCapacity != 2048) {
        pRedisParser->nCapacity = 2048;
        lua_newuserdatauv(L, pRedisParser->nCapacity, 0);
        lua_setiuservalue(L, 1, 1);
    }
    return 0;
}

static int32_t lwrite(struct lua_State* L)
{
    lredisParser_tt* pRedisParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pRedisParser != NULL, 1, "invalid user data");

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

    if (nLength == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_getiuservalue(L, 1, 1);
    char* pStreamBuffer = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (pRedisParser->nLength + nLength > pRedisParser->nCapacity) {
        pRedisParser->nCapacity = pRedisParser->nLength + nLength;
        char* p                 = lua_newuserdatauv(L, pRedisParser->nCapacity, 0);
        if (pRedisParser->nLength != 0) {
            memcpy(p, pStreamBuffer, pRedisParser->nLength);
        }
        lua_setiuservalue(L, 1, 1);
        pStreamBuffer = p;
    }

    memcpy(pStreamBuffer + pRedisParser->nLength, pBuffer, nLength);
    pRedisParser->nLength += nLength;
    int32_t iCount  = pRedisParser->iCount;
    size_t  nOffset = 0;
    do {
        size_t nRead = redis_parser_execute(&pRedisParser->parser,
                                            &s_redisParserSettings,
                                            pStreamBuffer + nOffset,
                                            pRedisParser->nLength - nOffset);
        if (nRead == 0) {
            if (REDIS_PARSER_ERRNO(&pRedisParser->parser) != redis_errno_ok) {
                lua_pushinteger(L, -1);
            }
            else {
                if (nOffset != 0) {
                    if (nOffset != pRedisParser->nLength) {
                        memmove(pStreamBuffer,
                                pStreamBuffer + nOffset,
                                pRedisParser->nLength - nOffset);
                    }
                    pRedisParser->nLength -= nOffset;
                }
                lua_pushinteger(L, pRedisParser->iCount - iCount);
            }
            return 1;
        }
        nOffset += nRead;
    } while (true);

    lua_pushinteger(L, 0);
    return 1;
}

static inline void lua_redisReplyArray(struct lua_State* L, redisReply_tt* pReply);

static inline void lua_redisReply(struct lua_State* L, redisReply_tt* pReply)
{
    switch (pReply->type) {
    case redis_reply_type_line:
    case redis_reply_type_bulk:
    case redis_reply_type_error:
    {
        replyString_tt* pValue = (replyString_tt*)pReply->value;
        if (pValue->length >= 0) {
            lua_pushlstring(L, pValue->value, pValue->length);
        }
        else {
            lua_pushnil(L);
        }
    } break;
    case redis_reply_type_integer:
    {
        int64_t* pValue = (int64_t*)pReply->value;
        lua_pushinteger(L, *pValue);
    } break;
    case redis_reply_type_array:
    {
        lua_redisReplyArray(L, pReply);
    } break;
    }
}

static inline void lua_redisReplyArray(struct lua_State* L, redisReply_tt* pReply)
{
    int64_t count = *(int64_t*)pReply->value;
    lua_createtable(L, count, 0);
    for (int64_t i = 0; i < count; ++i) {
        assert(!pReply->bEof);
        pReply = pReply->next;
        lua_redisReply(L, pReply);
        lua_rawseti(L, -2, i + 1);
    }
}

static int32_t lread(struct lua_State* L)
{
    lredisParser_tt* pRedisParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pRedisParser != NULL, 1, "invalid user data");
    if (pRedisParser->iCount <= 0) {
        return 0;
    }
    --pRedisParser->iCount;

    redisReply_tt* pNextReply = pRedisParser->pReply;
    while (pNextReply) {
        if (pNextReply->bEof) {
            redisReply_tt* pEnd = pNextReply;
            pNextReply          = pNextReply->next;
            pEnd->next          = NULL;
            break;
        }
        pNextReply = pNextReply->next;
    }

    redisReply_tt* pReply = pRedisParser->pReply;
    lua_redisReply(L, pReply);

    redisReply_free(pReply);
    pRedisParser->pReply = pNextReply;
    if (pRedisParser->pReply == NULL) {
        pRedisParser->pLastReply = NULL;
    }
    return 1;
}

int32_t luaopen_lruntime_redis(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_lredis[] = {
        {"new", lnew}, {"reset", lreset}, {"write", lwrite}, {"read", lread}, {NULL, NULL}};
    luaL_newlib(L, lualib_lredis);
    return 1;
}
