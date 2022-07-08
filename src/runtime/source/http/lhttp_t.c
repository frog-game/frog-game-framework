

#include "http/lhttp_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "http_parser.h"

#include "utility_t.h"

#define MAX_HEADERS 24
#define MAX_HEADER_SIZE 1024
#define MAX_ELEMENT_SIZE 2048

struct httpResult_s;
typedef struct httpResult_s httpResult_tt;

typedef struct httpResponse_s
{
    char     szStatus[MAX_ELEMENT_SIZE];
    uint32_t uiStatusCode;
} httpResponse_tt;

typedef struct httpRequest_s
{
    struct http_parser_url url;
    enum http_method       eMethod;
    char                   szRequestUrl[MAX_ELEMENT_SIZE];
} httpRequest_tt;

struct httpResult_s
{
    httpResult_tt* pNext;
    char*          pBody;
    size_t         nBodyLength;
    char           headers[MAX_HEADERS][2][MAX_HEADER_SIZE];
    int32_t        iHeaders;
    uint16_t       uiHttpMajor;
    uint16_t       uiHttpMinor;
    bool           bKeepAlive;
    bool           bUpgrade;
    char           pTypeData[];
};

static inline httpResult_tt* httpResult_new(bool bResponse)
{
    httpResult_tt* pResult = NULL;
    if (bResponse) {
        pResult                    = mem_malloc(sizeof(httpResult_tt) + sizeof(httpResponse_tt));
        httpResponse_tt* pResponse = (httpResponse_tt*)pResult->pTypeData;
        pResponse->uiStatusCode    = 0;
        bzero(pResponse->szStatus, MAX_ELEMENT_SIZE);
    }
    else {
        pResult                  = mem_malloc(sizeof(httpResult_tt) + sizeof(httpRequest_tt));
        httpRequest_tt* pRequest = (httpRequest_tt*)pResult->pTypeData;
        pRequest->eMethod        = 0;
        bzero(pRequest->szRequestUrl, MAX_ELEMENT_SIZE);
        http_parser_url_init(&pRequest->url);
    }
    pResult->pNext       = NULL;
    pResult->nBodyLength = 0;
    pResult->pBody       = NULL;
    bzero(pResult->headers, MAX_HEADERS * 2 * MAX_HEADER_SIZE);
    pResult->iHeaders    = 0;
    pResult->uiHttpMajor = 0;
    pResult->uiHttpMinor = 0;
    pResult->bKeepAlive  = false;
    pResult->bUpgrade    = false;
    return pResult;
}

static inline void httpResult_free(httpResult_tt* pResult)
{
    if (pResult->pNext) {
        httpResult_free(pResult->pNext);
        pResult->pNext = NULL;
    }
    if (pResult->pBody) {
        mem_free(pResult->pBody);
        pResult->pBody = NULL;
    }
    mem_free(pResult);
}

typedef struct lhttpParser_s
{
    http_parser    parser;
    bool           bResponse;
    bool           bLastHeaderField;
    bool           bParser;
    int32_t        iCount;
    size_t         nCapacity;
    size_t         nLength;
    httpResult_tt* pResult;
    httpResult_tt* pLastResult;
} lhttpParser_tt;

static inline int32_t onMessageBegin(struct http_parser* p)
{
    lhttpParser_tt* pHttpParser   = (lhttpParser_tt*)(p->data);
    pHttpParser->bLastHeaderField = true;

    httpResult_tt* pResult = httpResult_new(pHttpParser->bResponse);
    if (pHttpParser->pResult) {
        pHttpParser->pLastResult->pNext = pResult;
    }
    else {
        pHttpParser->pResult = pResult;
    }
    pHttpParser->pLastResult = pResult;
    return 0;
}

static inline int32_t onHeaderField(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);

    httpResult_tt* pResult = pHttpParser->pLastResult;

    if (!pHttpParser->bLastHeaderField) {
        pHttpParser->bLastHeaderField = true;
        ++pResult->iHeaders;
    }

    if (pResult->iHeaders >= MAX_HEADERS) {
        return -1;
    }

    if (strlncat(pResult->headers[pResult->iHeaders][0],
                 sizeof(pResult->headers[pResult->iHeaders][0]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static inline int32_t onHeaderValue(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpParser_tt* pHttpParser   = (lhttpParser_tt*)(p->data);
    pHttpParser->bLastHeaderField = false;

    httpResult_tt* pResult = pHttpParser->pLastResult;

    if (strlncat(pResult->headers[pResult->iHeaders][1],
                 sizeof(pResult->headers[pResult->iHeaders][1]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static inline int32_t onRequestUrl(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);
    if (!pHttpParser->bResponse) {
        httpResult_tt*  pResult  = pHttpParser->pLastResult;
        httpRequest_tt* pRequest = (httpRequest_tt*)pResult->pTypeData;
        if (strlncat(pRequest->szRequestUrl, sizeof(pRequest->szRequestUrl), pBuffer, nLength) ==
            0) {
            return -1;
        }
    }
    return 0;
}

static inline int32_t onResponseStatus(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);
    if (pHttpParser->bResponse) {
        httpResult_tt*   pResult   = pHttpParser->pLastResult;
        httpResponse_tt* pResponse = (httpResponse_tt*)pResult->pTypeData;
        if (strlncat(pResponse->szStatus, sizeof(pResponse->szStatus), pBuffer, nLength) == 0) {
            return -1;
        }
    }
    return 0;
}

static inline int32_t onBody(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);
    httpResult_tt*  pResult     = pHttpParser->pLastResult;
    if (pResult->pBody == NULL) {
        pResult->pBody = mem_malloc(nLength);
        memcpy(pResult->pBody, pBuffer, nLength);
    }
    else {
        pResult->pBody = mem_realloc(pResult->pBody, pResult->nBodyLength + nLength);
        memcpy(pResult->pBody + pResult->nBodyLength, pBuffer, nLength);
    }
    pResult->nBodyLength += nLength;
    return 0;
}

static inline int32_t onHeadersComplete(struct http_parser* p)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);
    httpResult_tt*  pResult     = pHttpParser->pLastResult;

    if (!pHttpParser->bResponse) {
        httpRequest_tt* pRequest = (httpRequest_tt*)pResult->pTypeData;

        if (http_parser_parse_url(
                pRequest->szRequestUrl, strlen(pRequest->szRequestUrl), 0, &pRequest->url) != 0) {
            return -1;
        }
        pRequest->eMethod = p->method;
    }
    else {
        httpResponse_tt* pResponse = (httpResponse_tt*)pResult->pTypeData;
        pResponse->uiStatusCode    = p->status_code;
    }
    pResult->uiHttpMajor = p->http_major;
    pResult->uiHttpMinor = p->http_minor;
    pResult->bKeepAlive  = http_should_keep_alive(p) == 1;
    return 0;
}

static inline int32_t onMessageComplete(struct http_parser* p)
{
    lhttpParser_tt* pHttpParser = (lhttpParser_tt*)(p->data);
    ++pHttpParser->iCount;
    return 0;
}

static http_parser_settings s_httpParserSettings = {.on_message_begin    = onMessageBegin,
                                                    .on_header_field     = onHeaderField,
                                                    .on_header_value     = onHeaderValue,
                                                    .on_url              = onRequestUrl,
                                                    .on_status           = onResponseStatus,
                                                    .on_body             = onBody,
                                                    .on_headers_complete = onHeadersComplete,
                                                    .on_message_complete = onMessageComplete,
                                                    .on_chunk_header     = NULL,
                                                    .on_chunk_complete   = NULL};

static int32_t lnew(struct lua_State* L)
{
    bool            bResponse   = lua_toboolean(L, 1);
    lhttpParser_tt* pHttpParser = lua_newuserdatauv(L, sizeof(lhttpParser_tt), 1);
    pHttpParser->parser.data    = pHttpParser;
    http_parser_init(&pHttpParser->parser, bResponse ? HTTP_RESPONSE : HTTP_REQUEST);
    pHttpParser->bResponse        = bResponse;
    pHttpParser->bLastHeaderField = true;
    pHttpParser->bParser          = true;
    pHttpParser->iCount           = 0;
    pHttpParser->nCapacity        = 2048;
    pHttpParser->nLength          = 0;
    pHttpParser->pResult          = NULL;
    pHttpParser->pLastResult      = NULL;

    lua_newuserdatauv(L, pHttpParser->nCapacity, 0);
    lua_setiuservalue(L, 2, 1);
    return 1;
}

static int32_t lclear(struct lua_State* L)
{
    lhttpParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");
    if (pHttpParser->pResult) {
        httpResult_free(pHttpParser->pResult);
        pHttpParser->pResult = NULL;
    }
    lua_pushnil(L);
    lua_setiuservalue(L, 1, 1);
    return 0;
}

static int32_t lreset(struct lua_State* L)
{
    lhttpParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");
    http_parser_init(&pHttpParser->parser, pHttpParser->bResponse ? HTTP_RESPONSE : HTTP_REQUEST);
    pHttpParser->bLastHeaderField = true;
    pHttpParser->bParser          = true;
    pHttpParser->iCount           = 0;
    pHttpParser->nLength          = 0;
    pHttpParser->pLastResult      = NULL;

    if (pHttpParser->pResult) {
        httpResult_free(pHttpParser->pResult);
        pHttpParser->pResult = NULL;
    }

    if (pHttpParser->nCapacity != 2048) {
        pHttpParser->nCapacity = 2048;
        lua_newuserdatauv(L, pHttpParser->nCapacity, 0);
        lua_setiuservalue(L, 1, 1);
    }
    return 0;
}

static int32_t lwrite(struct lua_State* L)
{
    lhttpParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");

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

    if (!pHttpParser->bParser) {
        return luaL_error(L, "pHttpParser write off");
    }

    lua_getiuservalue(L, 1, 1);
    char* pStreamBuffer = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (pHttpParser->nLength + nLength > pHttpParser->nCapacity) {
        pHttpParser->nCapacity = pHttpParser->nLength + nLength;
        char* p                = lua_newuserdatauv(L, pHttpParser->nCapacity, 0);
        if (pHttpParser->nLength != 0) {
            memcpy(p, pStreamBuffer, pHttpParser->nLength);
        }
        lua_setiuservalue(L, 1, 1);
        pStreamBuffer = p;
    }

    memcpy(pStreamBuffer + pHttpParser->nLength, pBuffer, nLength);
    pHttpParser->nLength += nLength;

    size_t nOffset = 0;

    int32_t iCount = pHttpParser->iCount;
    do {
        size_t nRead = http_parser_execute(&pHttpParser->parser,
                                           &s_httpParserSettings,
                                           pStreamBuffer + nOffset,
                                           nLength - nOffset);
        if (pHttpParser->parser.upgrade) {
            pHttpParser->bParser               = false;
            pHttpParser->pLastResult->bUpgrade = true;
            if (nLength - nOffset - nRead != 0) {
                pHttpParser->pLastResult->pBody = mem_malloc(nLength - nOffset - nRead);
                memcpy(pHttpParser->pLastResult->pBody,
                       pStreamBuffer + nOffset + nRead,
                       nLength - nOffset - nRead);
                pHttpParser->pLastResult->nBodyLength = nLength - nOffset - nRead;
            }
            pHttpParser->nLength = 0;
            lua_pushinteger(L, pHttpParser->iCount - iCount);
            return 1;
        }

        if (HTTP_PARSER_ERRNO(&pHttpParser->parser) != HPE_OK) {
            pHttpParser->bParser = false;
            lua_pushinteger(L, -1);
            return 1;
        }

        if (nRead == 0) {
            break;
        }
        nOffset += nRead;
    } while (nOffset < nLength);

    if (nOffset != 0) {
        if (nOffset != pHttpParser->nLength) {
            memmove(pStreamBuffer, pStreamBuffer + nOffset, pHttpParser->nLength - nOffset);
        }
        pHttpParser->nLength -= nOffset;
    }
    lua_pushinteger(L, pHttpParser->iCount - iCount);
    return 1;
}

static int32_t lread(struct lua_State* L)
{
    lhttpParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");
    if (pHttpParser->iCount <= 0) {
        return 0;
    }
    --pHttpParser->iCount;

    httpResult_tt* pResult = pHttpParser->pResult;
    httpResult_tt* pNext   = pResult->pNext;
    pResult->pNext         = NULL;

    if (!pHttpParser->bResponse) {
        lua_createtable(L, 0, 13);

        httpRequest_tt* pRequest = (httpRequest_tt*)pResult->pTypeData;
        lua_pushinteger(L, pRequest->eMethod);
        lua_setfield(L, -2, "_method");

        if (pRequest->url.field_set & (1 << UF_HOST)) {
            lua_pushlstring(L,
                            pRequest->szRequestUrl + pRequest->url.field_data[UF_HOST].off,
                            pRequest->url.field_data[UF_HOST].len);
            lua_setfield(L, -2, "_host");
        }

        if (pRequest->url.field_set & (1 << UF_PORT)) {
            lua_pushinteger(L, pRequest->url.port);
            lua_setfield(L, -2, "_port");
        }
        else {
            char szSchema[32];
            if (pRequest->url.field_set & (1 << UF_SCHEMA)) {
                if (strlncat(szSchema,
                             sizeof(szSchema),
                             pRequest->szRequestUrl + pRequest->url.field_data[UF_SCHEMA].off,
                             pRequest->url.field_data[UF_SCHEMA].len) == 0) {
                    bzero(szSchema, 32);
                }
            }
            else {
                bzero(szSchema, 32);
            }

            if (strcasecmp(szSchema, "https") == 0 || strcasecmp(szSchema, "wss") == 0) {
                lua_pushinteger(L, 443);
                lua_setfield(L, -2, "_port");
            }
            else {
                lua_pushinteger(L, 80);
                lua_setfield(L, -2, "_port");
            }
        }

        if (pRequest->url.field_set & (1 << UF_PATH)) {
            lua_pushlstring(L,
                            pRequest->szRequestUrl + pRequest->url.field_data[UF_PATH].off,
                            pRequest->url.field_data[UF_PATH].len);
            lua_setfield(L, -2, "_path");
        }

        if (pRequest->url.field_set & (1 << UF_QUERY)) {
            lua_pushlstring(L,
                            pRequest->szRequestUrl + pRequest->url.field_data[UF_QUERY].off,
                            pRequest->url.field_data[UF_QUERY].len);
            lua_setfield(L, -2, "_query");
        }

        if (pRequest->url.field_set & (1 << UF_FRAGMENT)) {
            lua_pushlstring(L,
                            pRequest->szRequestUrl + pRequest->url.field_data[UF_FRAGMENT].off,
                            pRequest->url.field_data[UF_FRAGMENT].len);
            lua_setfield(L, -2, "_fragment");
        }

        if (pRequest->url.field_set & (1 << UF_USERINFO)) {
            lua_pushlstring(L,
                            pRequest->szRequestUrl + pRequest->url.field_data[UF_USERINFO].off,
                            pRequest->url.field_data[UF_USERINFO].len);
            lua_setfield(L, -2, "_userinfo");
        }
    }
    else {
        lua_createtable(L, 0, 8);

        httpResponse_tt* pResponse = (httpResponse_tt*)pResult->pTypeData;

        lua_pushinteger(L, pResponse->uiStatusCode);
        lua_setfield(L, -2, "_status_code");

        lua_pushstring(L, pResponse->szStatus);
        lua_setfield(L, -2, "_status");
    }

    lua_pushinteger(L, pResult->uiHttpMajor);
    lua_setfield(L, -2, "_http_major");

    lua_pushinteger(L, pResult->uiHttpMinor);
    lua_setfield(L, -2, "_http_minor");

    lua_pushboolean(L, pResult->bKeepAlive ? 1 : 0);
    lua_setfield(L, -2, "_keep_alive");

    lua_pushboolean(L, pResult->bUpgrade ? 1 : 0);
    lua_setfield(L, -2, "_upgrade");

    if (pResult->iHeaders != 0) {
        lua_createtable(L, 0, pResult->iHeaders);
        luaL_checkstack(L, pResult->iHeaders, NULL);
        for (int32_t i = 0; i < pResult->iHeaders; ++i) {
            lua_pushstring(L, pResult->headers[i][1]);
            lua_setfield(L, -2, pResult->headers[i][0]);
        }
        lua_setfield(L, -2, "_headers");
    }
    else {
        lua_pushnil(L);
        lua_setfield(L, -2, "_headers");
    }

    if (pResult->nBodyLength != 0) {
        lua_pushlstring(L, pResult->pBody, pResult->nBodyLength);
        lua_setfield(L, -2, "_body");
    }
    else {
        lua_pushnil(L);
        lua_setfield(L, -2, "_body");
    }
    httpResult_free(pResult);
    pHttpParser->pResult = pNext;
    if (pHttpParser->pResult == NULL) {
        pHttpParser->pLastResult = NULL;
    }
    return 1;
}

int32_t luaopen_lruntime_http(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif

    luaL_Reg lualib_http[] = {{"new", lnew},
                              {"reset", lreset},
                              {"write", lwrite},
                              {"read", lread},
                              {"clear", lclear},
                              {NULL, NULL}};
    luaL_newlib(L, lualib_http);
    return 1;
}
