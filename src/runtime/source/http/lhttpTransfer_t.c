

#include "http/lhttpTransfer_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "http_parser.h"
#include "multipart_parser.h"

#include "byteQueue_t.h"
#include "utility_t.h"

#define MAX_HEADERS 24
#define MAX_HEADER_SIZE 1024
#define MAX_ELEMENT_SIZE 2048
#define MAX_CONTENTS 4
#define MAX_CONTENT_SIZE 1024
//#define MAX_CHUNKS 16

struct lhttpTransferParser_s;
typedef struct lhttpTransferParser_s lhttpTransferParser_tt;

typedef struct httpTransferResponse_s
{
    char     szStatus[MAX_ELEMENT_SIZE];
    uint32_t uiStatusCode;
} httpTransferResponse_tt;

typedef struct httpTransferRequest_s
{
    struct http_parser_url url;
    enum http_method       eMethod;
    char                   szRequestUrl[MAX_ELEMENT_SIZE];
} httpTransferRequest_tt;

struct httpMultipart_s;
typedef struct httpMultipart_s httpMultipart_tt;

struct httpMultipart_s
{
    char              content[MAX_CONTENTS][2][MAX_CONTENT_SIZE];
    int32_t           iContents;
    char*             pData;
    size_t            nLength;
    bool              bEof;
    httpMultipart_tt* pNext;
};

static inline void httpMultipart_free(httpMultipart_tt* pMultipart)
{
    if (pMultipart->pNext) {
        httpMultipart_free(pMultipart->pNext);
        pMultipart->pNext = NULL;
    }

    if (pMultipart->pData) {
        mem_free(pMultipart->pData);
    }
    mem_free(pMultipart);
}

struct lhttpTransferParser_s
{
    http_parser       parser;
    multipart_parser* pMultipartParse;
    httpMultipart_tt* pMultipart;
    httpMultipart_tt* pLastMultipart;
    char*             pBody;
    size_t            nBodyLength;
    uint16_t          uiHttpMajor;
    uint16_t          uiHttpMinor;
    const char*       szBoundary;
    int32_t           iHeaders;
    char              headers[MAX_HEADERS][2][MAX_HEADER_SIZE];
    union
    {
        httpTransferRequest_tt  request;
        httpTransferResponse_tt response;
    };
    bool bParser;
    bool bResponse;
    bool bKeepAlive;
    bool bUpgrade;
    bool bLastHeaderField;
    bool bHeaderComplete;
    bool bComplete;
};

static inline void lhttpTransfer_reset(lhttpTransferParser_tt* pHttpParser)
{
    http_parser_init(&pHttpParser->parser, pHttpParser->bResponse ? HTTP_RESPONSE : HTTP_REQUEST);
    pHttpParser->bLastHeaderField = true;
    pHttpParser->bParser          = true;

    bzero(pHttpParser->headers, MAX_HEADERS * 2 * MAX_HEADER_SIZE);
    pHttpParser->iHeaders       = 0;
    pHttpParser->bComplete      = false;
    pHttpParser->uiHttpMajor    = 0;
    pHttpParser->uiHttpMinor    = 0;
    pHttpParser->bKeepAlive     = false;
    pHttpParser->bUpgrade       = false;
    pHttpParser->szBoundary     = NULL;
    pHttpParser->pLastMultipart = NULL;
    if (pHttpParser->bResponse) {
        pHttpParser->response.uiStatusCode = 0;
        bzero(pHttpParser->response.szStatus, MAX_ELEMENT_SIZE);
    }
    else {
        pHttpParser->request.eMethod = 0;
        bzero(pHttpParser->request.szRequestUrl, MAX_ELEMENT_SIZE);
        http_parser_url_init(&pHttpParser->request.url);
    }

    if (pHttpParser->pBody) {
        mem_free(pHttpParser->pBody);
        pHttpParser->pBody = NULL;
    }

    if (pHttpParser->pMultipartParse) {
        multipart_parser_free(pHttpParser->pMultipartParse);
        pHttpParser->pMultipartParse = NULL;
    }

    if (pHttpParser->pMultipart) {
        httpMultipart_free(pHttpParser->pMultipart);
        pHttpParser->pMultipart = NULL;
    }
}

static int32_t onMessageBegin(struct http_parser* p)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    pHttpParser->bLastHeaderField       = true;
    return 0;
}

static int32_t onHeaderField(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);

    if (!pHttpParser->bLastHeaderField) {
        pHttpParser->bLastHeaderField = true;
        ++pHttpParser->iHeaders;
    }

    if (pHttpParser->iHeaders >= MAX_HEADERS) {
        return -1;
    }

    if (strlncat(pHttpParser->headers[pHttpParser->iHeaders][0],
                 sizeof(pHttpParser->headers[pHttpParser->iHeaders][0]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static int32_t onHeaderValue(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    pHttpParser->bLastHeaderField       = false;
    if (strlncat(pHttpParser->headers[pHttpParser->iHeaders][1],
                 sizeof(pHttpParser->headers[pHttpParser->iHeaders][1]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static int32_t onRequestUrl(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    if (!pHttpParser->bResponse) {
        if (strlncat(pHttpParser->request.szRequestUrl,
                     sizeof(pHttpParser->request.szRequestUrl),
                     pBuffer,
                     nLength) == 0) {
            return -1;
        }
    }

    return 0;
}

static int32_t onResponseStatus(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    if (pHttpParser->bResponse) {
        if (strlncat(pHttpParser->response.szStatus,
                     sizeof(pHttpParser->response.szStatus),
                     pBuffer,
                     nLength) == 0) {
            return -1;
        }
    }
    return 0;
}

static int32_t onBody(struct http_parser* p, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    if (pHttpParser->pBody == NULL) {
        pHttpParser->pBody = mem_malloc(nLength);
        memcpy(pHttpParser->pBody, pBuffer, nLength);
    }
    else {
        pHttpParser->pBody = mem_realloc(pHttpParser->pBody, pHttpParser->nBodyLength + nLength);
        memcpy(pHttpParser->pBody + pHttpParser->nBodyLength, pBuffer, nLength);
    }
    pHttpParser->nBodyLength += nLength;
    return 0;
}

static int32_t onHeadersComplete(struct http_parser* p)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    pHttpParser->bHeaderComplete        = true;

    if (!pHttpParser->bResponse) {
        if (http_parser_parse_url(pHttpParser->request.szRequestUrl,
                                  strlen(pHttpParser->request.szRequestUrl),
                                  0,
                                  &pHttpParser->request.url) != 0) {
            return -1;
        }
        pHttpParser->request.eMethod = p->method;
    }
    else {
        pHttpParser->response.uiStatusCode = p->status_code;
    }

    pHttpParser->uiHttpMajor = p->http_major;
    pHttpParser->uiHttpMinor = p->http_minor;
    pHttpParser->bKeepAlive  = http_should_keep_alive(p) == 1;

    pHttpParser->szBoundary = NULL;
    for (int32_t i = 0; i < pHttpParser->iHeaders; ++i) {
        if (strcmp(pHttpParser->headers[i][0], "Content-Type")) {
            pHttpParser->szBoundary = strchr(pHttpParser->headers[i][1], '=');
            break;
        }
    }

    return 0;
}

static int32_t onMessageComplete(struct http_parser* p)
{
    lhttpTransferParser_tt* pHttpParser = (lhttpTransferParser_tt*)(p->data);
    pHttpParser->bComplete              = true;
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
    bool                    bResponse   = lua_toboolean(L, 1);
    lhttpTransferParser_tt* pHttpParser = lua_newuserdatauv(L, sizeof(lhttpTransferParser_tt), 0);
    pHttpParser->bResponse              = bResponse;
    pHttpParser->parser.data            = pHttpParser;
    http_parser_init(&pHttpParser->parser, bResponse ? HTTP_RESPONSE : HTTP_REQUEST);
    pHttpParser->bLastHeaderField = true;
    pHttpParser->bParser          = true;
    pHttpParser->nBodyLength      = 0;
    pHttpParser->pBody            = NULL;
    bzero(pHttpParser->headers, MAX_HEADERS * 2 * MAX_ELEMENT_SIZE);
    pHttpParser->iHeaders        = 0;
    pHttpParser->bComplete       = false;
    pHttpParser->uiHttpMajor     = 0;
    pHttpParser->uiHttpMinor     = 0;
    pHttpParser->bKeepAlive      = false;
    pHttpParser->bUpgrade        = false;
    pHttpParser->pMultipartParse = NULL;
    pHttpParser->pMultipart      = NULL;
    pHttpParser->pLastMultipart  = NULL;

    if (pHttpParser->bResponse) {
        pHttpParser->response.uiStatusCode = 0;
        bzero(pHttpParser->response.szStatus, MAX_ELEMENT_SIZE);
    }
    else {
        pHttpParser->request.eMethod = 0;
        bzero(pHttpParser->request.szRequestUrl, MAX_ELEMENT_SIZE);
        http_parser_url_init(&pHttpParser->request.url);
    }
    return 1;
}

static int32_t lreset(struct lua_State* L)
{
    lhttpTransferParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");
    lhttpTransfer_reset(pHttpParser);
    return 0;
}

static int32_t lclear(struct lua_State* L)
{
    lhttpTransferParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");
    if (pHttpParser->pBody) {
        mem_free(pHttpParser->pBody);
        pHttpParser->pBody = NULL;
    }

    if (pHttpParser->pMultipartParse) {
        multipart_parser_free(pHttpParser->pMultipartParse);
        pHttpParser->pMultipartParse = NULL;
    }

    if (pHttpParser->pMultipart) {
        httpMultipart_free(pHttpParser->pMultipart);
        pHttpParser->pMultipart = NULL;
    }
    return 0;
}

static int32_t lwrite(struct lua_State* L)
{
    lhttpTransferParser_tt* pHttpParser = lua_touserdata(L, 1);
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

    if (pHttpParser->bComplete) {
        return luaL_error(L, "pHttpParser complete write error");
    }

    size_t nOffset = 0;

    do {
        size_t nRead = http_parser_execute(
            &pHttpParser->parser, &s_httpParserSettings, pBuffer + nOffset, nLength - nOffset);
        if (pHttpParser->parser.upgrade) {
            pHttpParser->bParser  = false;
            pHttpParser->bUpgrade = true;
            lua_pushinteger(L, 1);
            return 1;
        }

        if (HTTP_PARSER_ERRNO(&pHttpParser->parser) != HPE_OK) {
            pHttpParser->bParser = false;
            lua_pushinteger(L, -1);
            return 1;
        }

        nOffset += nRead;
        if (pHttpParser->bComplete) {
            lua_pushinteger(L, 1);
            return 1;
        }
        else {
            lua_pushinteger(L, 1);
            return 1;
        }

    } while (true);

    lua_pushinteger(L, 0);
    return 1;
}

static int32_t lreadHeader(struct lua_State* L)
{
    lhttpTransferParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");

    if (!pHttpParser->bHeaderComplete) {
        return 0;
    }

    if (!pHttpParser->bResponse) {
        lua_createtable(L, 0, 12);

        lua_pushinteger(L, pHttpParser->request.eMethod);
        lua_setfield(L, -2, "_method");

        if (pHttpParser->request.url.field_set & (1 << UF_HOST)) {
            lua_pushlstring(L,
                            pHttpParser->request.szRequestUrl +
                                pHttpParser->request.url.field_data[UF_HOST].off,
                            pHttpParser->request.url.field_data[UF_HOST].len);
            lua_setfield(L, -2, "_host");
        }

        if (pHttpParser->request.url.field_set & (1 << UF_PORT)) {
            lua_pushinteger(L, pHttpParser->request.url.port);
            lua_setfield(L, -2, "_port");
        }
        else {
            char szSchema[32];
            if (pHttpParser->request.url.field_set & (1 << UF_SCHEMA)) {
                if (strlncat(szSchema,
                             sizeof(szSchema),
                             pHttpParser->request.szRequestUrl +
                                 pHttpParser->request.url.field_data[UF_SCHEMA].off,
                             pHttpParser->request.url.field_data[UF_SCHEMA].len) == 0) {
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

        if (pHttpParser->request.url.field_set & (1 << UF_PATH)) {
            lua_pushlstring(L,
                            pHttpParser->request.szRequestUrl +
                                pHttpParser->request.url.field_data[UF_PATH].off,
                            pHttpParser->request.url.field_data[UF_PATH].len);
            lua_setfield(L, -2, "_path");
        }

        if (pHttpParser->request.url.field_set & (1 << UF_QUERY)) {
            lua_pushlstring(L,
                            pHttpParser->request.szRequestUrl +
                                pHttpParser->request.url.field_data[UF_QUERY].off,
                            pHttpParser->request.url.field_data[UF_QUERY].len);
            lua_setfield(L, -2, "_query");
        }

        if (pHttpParser->request.url.field_set & (1 << UF_FRAGMENT)) {
            lua_pushlstring(L,
                            pHttpParser->request.szRequestUrl +
                                pHttpParser->request.url.field_data[UF_FRAGMENT].off,
                            pHttpParser->request.url.field_data[UF_FRAGMENT].len);
            lua_setfield(L, -2, "_fragment");
        }

        if (pHttpParser->request.url.field_set & (1 << UF_USERINFO)) {
            lua_pushlstring(L,
                            pHttpParser->request.szRequestUrl +
                                pHttpParser->request.url.field_data[UF_USERINFO].off,
                            pHttpParser->request.url.field_data[UF_USERINFO].len);
            lua_setfield(L, -2, "_userinfo");
        }
    }
    else {
        lua_createtable(L, 0, 7);

        lua_pushinteger(L, pHttpParser->response.uiStatusCode);
        lua_setfield(L, -2, "_status_code");

        lua_pushstring(L, pHttpParser->response.szStatus);
        lua_setfield(L, -2, "_status");
    }

    lua_pushinteger(L, pHttpParser->uiHttpMajor);
    lua_setfield(L, -2, "_http_major");

    lua_pushinteger(L, pHttpParser->uiHttpMinor);
    lua_setfield(L, -2, "_http_minor");

    lua_pushboolean(L, pHttpParser->bKeepAlive ? 1 : 0);
    lua_setfield(L, -2, "_keep_alive");

    lua_pushboolean(L, pHttpParser->bUpgrade ? 1 : 0);
    lua_setfield(L, -2, "_upgrade");

    if (pHttpParser->iHeaders != 0) {
        lua_createtable(L, 0, pHttpParser->iHeaders);
        luaL_checkstack(L, pHttpParser->iHeaders, NULL);
        for (int32_t i = 0; i < pHttpParser->iHeaders; ++i) {
            lua_pushstring(L, pHttpParser->headers[i][1]);
            lua_setfield(L, -2, pHttpParser->headers[i][0]);
        }
        lua_setfield(L, -2, "_headers");
    }
    else {
        lua_pushnil(L);
        lua_setfield(L, -2, "_headers");
    }
    return 1;
}

static int32_t onMultipartPartDataBegin(multipart_parser* pParser)
{
    lhttpTransferParser_tt* pHttpParser =
        (lhttpTransferParser_tt*)multipart_parser_get_data(pParser);
    pHttpParser->bLastHeaderField = true;
    httpMultipart_tt* pMultipart  = mem_malloc(sizeof(httpMultipart_tt));
    bzero(pMultipart->content, MAX_CONTENTS * 2 * MAX_CONTENT_SIZE);
    pMultipart->iContents = 0;
    pMultipart->pData     = NULL;
    pMultipart->nLength   = 0;
    pMultipart->bEof      = false;
    pMultipart->pNext     = NULL;

    if (pHttpParser->pMultipart) {
        pHttpParser->pLastMultipart->pNext = pMultipart;
    }
    else {
        pHttpParser->pMultipart = pMultipart;
    }
    pHttpParser->pLastMultipart = pMultipart;

    return 0;
}

static int32_t onMultipartBodyEnd(multipart_parser* pParser)
{
    return 0;
}

static int32_t onMultipartField(multipart_parser* pParser, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser =
        (lhttpTransferParser_tt*)multipart_parser_get_data(pParser);
    httpMultipart_tt* pMultipart = pHttpParser->pLastMultipart;

    if (!pHttpParser->bLastHeaderField) {
        pHttpParser->bLastHeaderField = true;
        ++pMultipart->iContents;
    }

    if (pMultipart->iContents >= MAX_CONTENTS) {
        return -1;
    }

    if (strlncat(pMultipart->content[pMultipart->iContents][0],
                 sizeof(pMultipart->content[pMultipart->iContents][0]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static int32_t onMultipartValue(multipart_parser* pParser, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser =
        (lhttpTransferParser_tt*)multipart_parser_get_data(pParser);
    httpMultipart_tt* pMultipart  = pHttpParser->pLastMultipart;
    pHttpParser->bLastHeaderField = false;
    if (strlncat(pMultipart->content[pMultipart->iContents][1],
                 sizeof(pMultipart->content[pMultipart->iContents][1]),
                 pBuffer,
                 nLength) == 0) {
        return -1;
    }
    return 0;
}

static int32_t onMultipartPartData(multipart_parser* pParser, const char* pBuffer, size_t nLength)
{
    lhttpTransferParser_tt* pHttpParser =
        (lhttpTransferParser_tt*)multipart_parser_get_data(pParser);
    httpMultipart_tt* pMultipart = pHttpParser->pLastMultipart;
    if (pMultipart->pData == NULL) {
        pMultipart->pData = mem_malloc(nLength);
        memcpy(pMultipart->pData, pBuffer, nLength);
    }
    else {
        pMultipart->pData = mem_realloc(pMultipart->pData, pMultipart->nLength + nLength);
        memcpy(pMultipart->pData + pMultipart->nLength, pBuffer, nLength);
    }
    pMultipart->nLength += nLength;
    return 0;
}

static int32_t onMultipartPartDataEnd(multipart_parser* pParser)
{
    return 0;
}

static inline void* multipart_malloc(size_t size)
{
    return mem_malloc(size);
}

static inline void multipart_free(void* p)
{
    mem_free(p);
}

static multipart_parser_settings s_multipart_settings = {.on_header_field = onMultipartField,
                                                         .on_header_value = onMultipartValue,
                                                         .on_part_data    = onMultipartPartData,
                                                         .on_part_data_begin =
                                                             onMultipartPartDataBegin,
                                                         .on_part_data_end = onMultipartPartDataEnd,
                                                         .on_body_end      = onMultipartBodyEnd,
                                                         .on_headers_complete = NULL,
                                                         .on_malloc           = multipart_malloc,
                                                         .on_free             = multipart_free};

static int32_t lreadBody(struct lua_State* L)
{
    lhttpTransferParser_tt* pHttpParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pHttpParser != NULL, 1, "invalid user data");

    if (pHttpParser->szBoundary) {
        if (pHttpParser->pMultipartParse == NULL) {
            pHttpParser->pMultipartParse =
                multipart_parser_init(pHttpParser->szBoundary + 1, &s_multipart_settings);
            multipart_parser_set_data(pHttpParser->pMultipartParse, pHttpParser);
        }

        if (pHttpParser->nBodyLength != 0) {
            size_t nRead = multipart_parser_execute(
                pHttpParser->pMultipartParse, pHttpParser->pBody, pHttpParser->nBodyLength);
            if (nRead != pHttpParser->nBodyLength) {
                if (nRead != 0) {
                    memmove(pHttpParser->pBody,
                            pHttpParser->pBody + nRead,
                            pHttpParser->nBodyLength - nRead);
                }
            }
            else {
                mem_free(pHttpParser->pBody);
                pHttpParser->pBody       = NULL;
                pHttpParser->nBodyLength = 0;
            }
        }

        httpMultipart_tt* pMultipart = pHttpParser->pMultipart;
        if (pMultipart == NULL) {
            if (pHttpParser->bComplete) {
                lua_pushboolean(L, 1);
                lhttpTransfer_reset(pHttpParser);
            }
            else {
                lua_pushboolean(L, 0);
            }
            return 1;
        }

        httpMultipart_tt* pNext = pMultipart->pNext;
        pMultipart->pNext       = NULL;

        if (pNext == NULL && pHttpParser->bComplete) {
            lua_pushboolean(L, 1);
        }
        else {
            lua_pushboolean(L, 0);
        }

        lua_createtable(L, 0, 2);

        if (pMultipart->iContents != 0) {
            lua_createtable(L, 0, pMultipart->iContents);
            for (int32_t i = 0; i < pMultipart->iContents; ++i) {
                lua_pushstring(L, pMultipart->content[i][0]);
                lua_pushstring(L, pMultipart->content[i][1]);
                lua_rawset(L, -3);
            }
            lua_setfield(L, -2, "_contents");
        }
        else {
            lua_pushnil(L);
            lua_setfield(L, -2, "_contents");
        }

        if (pMultipart->nLength > 0) {
            lua_pushlstring(L, pMultipart->pData, pMultipart->nLength);
            lua_setfield(L, -2, "_body");
        }
        else {
            lua_pushnil(L);
            lua_setfield(L, -2, "_body");
        }

        httpMultipart_free(pMultipart);
        pHttpParser->pMultipart = pNext;
        if (pHttpParser->pMultipart == NULL) {
            pHttpParser->pLastMultipart = NULL;
        }
        return 2;
    }

    if (pHttpParser->bComplete) {
        lua_pushboolean(L, 1);
        if (pHttpParser->nBodyLength == 0) {
            return 1;
        }
        lua_pushlstring(L, pHttpParser->pBody, pHttpParser->nBodyLength);
        lhttpTransfer_reset(pHttpParser);
        return 2;
    }

    lua_pushboolean(L, 0);
    if (pHttpParser->nBodyLength == 0) {
        return 1;
    }

    lua_pushlstring(L, pHttpParser->pBody, pHttpParser->nBodyLength);
    pHttpParser->nBodyLength = 0;
    mem_free(pHttpParser->pBody);
    pHttpParser->pBody = NULL;
    return 2;
}

int32_t luaopen_lruntime_httpTransfer(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif

    luaL_Reg lualib_httpTransfer[] = {{"new", lnew},
                                      {"reset", lreset},
                                      {"write", lwrite},
                                      {"readHeader", lreadHeader},
                                      {"readBody", lreadBody},
                                      {"clear", lclear},
                                      {NULL, NULL}};
    luaL_newlib(L, lualib_httpTransfer);
    return 1;
}
