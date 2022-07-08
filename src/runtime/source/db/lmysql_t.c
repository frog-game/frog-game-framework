

#include "db/lmysql_t.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "log_t.h"
#include "utility_t.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "mysql_parser.h"

#include "stream/mysqlStream_t.h"

static int32_t lhandle(struct lua_State* L)
{
    lua_pushlightuserdata(L, mysqlStreamCreate());
    return 1;
}

typedef enum
{
    mysql_result_type_handshake,
    mysql_result_type_set,
    mysql_result_type_ok,
    mysql_result_type_error
} mysql_result_type_tt;

struct mysqlResult_s;
typedef struct mysqlResult_s mysqlResult_tt;

typedef struct cell_s
{
    const char* value;
    size_t      len;
} cell_tt;

typedef struct row_s
{
    cell_tt*      cell;
    char*         valueArray;
    struct row_s* next;
} row_tt;

static inline void row_free(row_tt* p)
{
    mem_free(p->valueArray);
    p->valueArray = NULL;

    mem_free(p->cell);
    p->cell = NULL;
    if (p->next) {
        row_free(p->next);
        p->next = NULL;
    }
    mem_free(p);
}

typedef struct field_s
{
    char*   name;
    uint8_t type;
} field_tt;

typedef struct resultTypeSet_s
{
    uint32_t  fieldCount;
    uint32_t  rowCount;
    field_tt* field;
    row_tt*   row;
    row_tt*   lastRow;
} resultTypeSet_tt;

typedef struct resultTypeOK_s
{
    uint64_t affectedRows;
    uint64_t insertId;
    uint16_t warningCount;
} resultTypeOK_tt;

typedef struct resultTypeError_s
{
    char*    errorMsg;
    size_t   msgLength;
    char     sqlState[MYSQL_STATE_LENGTH];
    uint16_t errnoCode;
} resultTypeError_tt;

struct mysqlResult_s
{
    mysql_result_type_tt type;
    mysqlResult_tt*      next;
    bool                 bEof;
    char                 result[];
};

static inline mysqlResult_tt* mysqlResult_new(mysql_result_type_tt type)
{
    mysqlResult_tt* p = NULL;
    switch (type) {
    case mysql_result_type_handshake:
    {
        p       = mem_malloc(sizeof(mysqlResult_tt) + 20);
        p->type = mysql_result_type_handshake;
        p->next = NULL;
        p->bEof = true;
    } break;
    case mysql_result_type_set:
    {
        p                         = mem_malloc(sizeof(mysqlResult_tt) + sizeof(resultTypeSet_tt));
        p->type                   = mysql_result_type_set;
        p->next                   = NULL;
        p->bEof                   = false;
        resultTypeSet_tt* pResult = (resultTypeSet_tt*)p->result;
        pResult->fieldCount       = 0;
        pResult->rowCount         = 0;
        pResult->field            = NULL;
        pResult->row              = NULL;
        pResult->lastRow          = NULL;
    } break;
    case mysql_result_type_ok:
    {
        p                        = mem_malloc(sizeof(mysqlResult_tt) + sizeof(resultTypeOK_tt));
        p->type                  = mysql_result_type_ok;
        p->next                  = NULL;
        p->bEof                  = false;
        resultTypeOK_tt* pResult = (resultTypeOK_tt*)p->result;
        pResult->affectedRows    = 0;
        pResult->insertId        = 0;
        pResult->warningCount    = 0;
    } break;
    case mysql_result_type_error:
    {
        p       = mem_malloc(sizeof(mysqlResult_tt) + sizeof(resultTypeError_tt));
        p->type = mysql_result_type_error;
        p->next = NULL;
        p->bEof = false;
        resultTypeError_tt* pResult = (resultTypeError_tt*)p->result;
        pResult->errorMsg           = NULL;
        pResult->msgLength          = 0;
        pResult->errnoCode          = 0;
        bzero(pResult->sqlState, MYSQL_STATE_LENGTH);
    } break;
    }
    return p;
}

static inline void mysqlResult_free(mysqlResult_tt* p)
{
    if (p->next) {
        mysqlResult_free(p->next);
        p->next = NULL;
    }

    switch (p->type) {
    case mysql_result_type_ok:
    case mysql_result_type_handshake:
    {
        mem_free(p);
    } break;
    case mysql_result_type_set:
    {
        resultTypeSet_tt* pResultTypeSet = (resultTypeSet_tt*)p->result;

        for (uint32_t i = 0; i < pResultTypeSet->fieldCount; ++i) {
            mem_free(pResultTypeSet->field[i].name);
        }
        mem_free(pResultTypeSet->field);
        pResultTypeSet->field = NULL;

        if (pResultTypeSet->row) {
            row_free(pResultTypeSet->row);
            pResultTypeSet->row = NULL;
        }
        mem_free(p);
    } break;
    case mysql_result_type_error:
    {
        resultTypeError_tt* pResultTypeError = (resultTypeError_tt*)p->result;
        mem_free(pResultTypeError->errorMsg);
        mem_free(p);
    } break;
    }
}

typedef struct lmysqlParser_s
{
    mysql_parser_tt parser;
    int32_t         iCount;
    size_t          nPayloadLength;
    size_t          nStreamCapacity;
    size_t          nStreamLength;
    mysqlResult_tt* pResult;
    mysqlResult_tt* pLastResult;
} lmysqlParser_tt;

static int32_t mysql_handshake(mysql_parser_tt* p, uint8_t protocolVersion,
                               const char* serverVersion, uint32_t threadId, const char* scramble,
                               uint32_t serverCapabilities, uint8_t serverLang,
                               uint16_t serverStatus)
{
    lmysqlParser_tt* pParser = mysql_parser_get_data(p);
    pParser->pResult         = mysqlResult_new(mysql_result_type_handshake);
    memcpy(pParser->pResult->result, scramble, 20);
    pParser->pLastResult = pParser->pResult;
    return 0;
}

static int32_t mysql_ok(mysql_parser_tt* p, uint64_t affectedRows, uint64_t insertId,
                        uint16_t serverStatus, uint16_t warningCount, const char* message,
                        size_t messageLength)
{
    lmysqlParser_tt* pParser = mysql_parser_get_data(p);

    mysqlResult_tt* pResult = mysqlResult_new(mysql_result_type_ok);

    resultTypeOK_tt* pResultOK = (resultTypeOK_tt*)pResult->result;
    pResultOK->affectedRows    = affectedRows;
    pResultOK->insertId        = insertId;
    pResultOK->warningCount    = warningCount;

    if (pParser->pResult) {
        pParser->pLastResult->next = pResult;
    }
    else {
        pParser->pResult = pResult;
    }
    pParser->pLastResult = pResult;
    return 0;
}

static int32_t mysql_error(mysql_parser_tt* p, uint16_t errnoCode, size_t errorMsgLength,
                           const char* errorMsg, const char* state)
{
    lmysqlParser_tt*    pParser      = mysql_parser_get_data(p);
    mysqlResult_tt*     pResult      = mysqlResult_new(mysql_result_type_error);
    resultTypeError_tt* pResultError = (resultTypeError_tt*)pResult->result;
    pResultError->errnoCode          = errnoCode;
    if (errorMsgLength > 0) {
        pResultError->errorMsg = mem_malloc(errorMsgLength);
        memcpy(pResultError->errorMsg, errorMsg, errorMsgLength);
        pResultError->msgLength = errorMsgLength;
    }
    memcpy(pResultError->sqlState, state, MYSQL_STATE_LENGTH);
    if (pParser->pResult) {
        pParser->pLastResult->next = pResult;
    }
    else {
        pParser->pResult = pResult;
    }
    pParser->pLastResult = pResult;

    return 0;
}

static int32_t mysql_field_count(mysql_parser_tt* p, uint32_t fieldCount)
{
    lmysqlParser_tt*  pParser    = mysql_parser_get_data(p);
    mysqlResult_tt*   pResult    = mysqlResult_new(mysql_result_type_set);
    resultTypeSet_tt* pResultSet = (resultTypeSet_tt*)pResult->result;
    pResultSet->fieldCount       = fieldCount;
    pResultSet->field            = mem_malloc(fieldCount * sizeof(field_tt));
    bzero(pResultSet->field, fieldCount * sizeof(field_tt));
    if (pParser->pResult) {
        pParser->pLastResult->next = pResult;
    }
    else {
        pParser->pResult = pResult;
    }
    pParser->pLastResult = pResult;
    return 0;
}

static int32_t mysql_column_def(mysql_parser_tt* p, uint32_t index, size_t catalogLength,
                                const char* catalog, size_t DBLength, const char* DB,
                                size_t tableLength, const char* table, size_t orgTableLength,
                                const char* orgTable, size_t nameLength, const char* name,
                                size_t orgNameLength, const char* orgName, uint32_t fieldLength,
                                uint16_t fieldCharsetnr, uint16_t fieldFlag, uint8_t type,
                                uint8_t decimals)
{
    lmysqlParser_tt* pParser = mysql_parser_get_data(p);
    assert(pParser->pLastResult && (pParser->pLastResult->type == mysql_result_type_set));
    resultTypeSet_tt* pResultSet = (resultTypeSet_tt*)pParser->pLastResult->result;
    assert(index < pResultSet->fieldCount);
    pResultSet->field[index].type = type;
    assert(nameLength > 0);
    pResultSet->field[index].name = mem_malloc(nameLength + 1);
    memcpy(pResultSet->field[index].name, name, nameLength);
    pResultSet->field[index].name[nameLength] = '\0';
    return 0;
}

static int32_t mysql_row(mysql_parser_tt* p, const char* buf, size_t length, size_t* valueOffset,
                         size_t* valueLength)
{
    lmysqlParser_tt* pParser = mysql_parser_get_data(p);
    assert(pParser->pLastResult && (pParser->pLastResult->type == mysql_result_type_set));
    resultTypeSet_tt* pResultSet = (resultTypeSet_tt*)pParser->pLastResult->result;

    ++pResultSet->rowCount;
    row_tt* row = mem_malloc(sizeof(row_tt));
    row->next   = NULL;
    if (length > 0) {
        row->valueArray = mem_malloc(length);
        memcpy(row->valueArray, buf, length);
    }
    else {
        row->valueArray = NULL;
    }
    row->cell = mem_malloc(sizeof(cell_tt) * pResultSet->fieldCount);
    bzero(row->cell, pResultSet->fieldCount * sizeof(cell_tt));
    for (uint32_t i = 0; i < pResultSet->fieldCount; i++) {
        row->cell[i].value = row->valueArray + valueOffset[i];
        row->cell[i].len   = valueLength[i];
    }
    if (pResultSet->row) {
        pResultSet->lastRow->next = row;
    }
    else {
        pResultSet->row = row;
    }
    pResultSet->lastRow = row;

    return 0;
}

static int32_t mysql_complete(mysql_parser_tt* p)
{
    lmysqlParser_tt* pParser = mysql_parser_get_data(p);
    assert(pParser->pLastResult);
    ++pParser->iCount;
    pParser->pLastResult->bEof = true;
    return 0;
}

static mysql_parser_settings_tt s_mysqlParserSettings = {.on_complete    = mysql_complete,
                                                         .on_handshake   = mysql_handshake,
                                                         .on_ok          = mysql_ok,
                                                         .on_error       = mysql_error,
                                                         .on_field_count = mysql_field_count,
                                                         .on_column_def  = mysql_column_def,
                                                         .on_row         = mysql_row};

static int32_t lnew(struct lua_State* L)
{
    lmysqlParser_tt* pMysqlParser = lua_newuserdatauv(L, sizeof(lmysqlParser_tt), 1);
    mysql_parser_init(&pMysqlParser->parser, mysql_parser_type_handshake, pMysqlParser);
    pMysqlParser->nStreamCapacity = 2048;
    pMysqlParser->nStreamLength   = 0;

    pMysqlParser->nPayloadLength = 0;
    pMysqlParser->iCount         = 0;
    pMysqlParser->pResult        = NULL;
    pMysqlParser->pLastResult    = NULL;

    lua_newuserdatauv(L, pMysqlParser->nStreamCapacity, 0);
    lua_setiuservalue(L, 1, 1);

    return 1;
}

static int32_t lreset(struct lua_State* L)
{
    lmysqlParser_tt* pMysqlParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pMysqlParser != NULL, 1, "invalid user data");
    mysql_parser_type_tt type =
        lua_toboolean(L, 2) == 1 ? mysql_parser_type_handshake : mysql_parser_type_result;
    mysql_parser_reset(&pMysqlParser->parser, (mysql_parser_type_tt)type);
    pMysqlParser->nStreamLength = 0;
    pMysqlParser->iCount        = 0;
    pMysqlParser->pLastResult   = NULL;
    if (pMysqlParser->pResult) {
        mysqlResult_free(pMysqlParser->pResult);
        pMysqlParser->pResult = NULL;
    }

    if (pMysqlParser->nStreamCapacity != 2048) {
        pMysqlParser->nStreamCapacity = 2048;
        lua_newuserdatauv(L, pMysqlParser->nStreamCapacity, 0);
        lua_setiuservalue(L, 1, 1);
    }

    return 0;
}

inline static char* writeStreamBuffer(struct lua_State* L, lmysqlParser_tt* pMysqlParser,
                                      char* pStreamBuffer, const char* pBuffer, size_t nLength)
{
    if (pMysqlParser->nStreamLength + nLength > pMysqlParser->nStreamCapacity) {
        pMysqlParser->nStreamCapacity = pMysqlParser->nStreamLength + nLength;
        char* p                       = lua_newuserdatauv(L, pMysqlParser->nStreamCapacity, 0);
        if (pMysqlParser->nStreamLength != 0) {
            memcpy(p, pStreamBuffer, pMysqlParser->nStreamLength);
        }
        lua_setiuservalue(L, 1, 1);
        pStreamBuffer = p;
    }

    memcpy(pStreamBuffer + pMysqlParser->nStreamLength, pBuffer, nLength);
    pMysqlParser->nStreamLength += nLength;
    return pStreamBuffer;
}

static int32_t lwrite(struct lua_State* L)
{
    lmysqlParser_tt* pMysqlParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pMysqlParser != NULL, 1, "invalid user data");

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

    int32_t iCount = pMysqlParser->iCount;

    lua_getiuservalue(L, 1, 1);
    char* pStreamBuffer = lua_touserdata(L, -1);
    lua_pop(L, 1);

    size_t nOffset = 0;

    if (pMysqlParser->nStreamLength > pMysqlParser->nPayloadLength) {
        size_t nHead = pMysqlParser->nStreamLength - pMysqlParser->nPayloadLength;
        nOffset      = sizeof(uint32_t) - nHead;
        if (nOffset > nLength) {
            writeStreamBuffer(L, pMysqlParser, pStreamBuffer, pBuffer, nLength);
            lua_pushinteger(L, 0);
            return 1;
        }
        pStreamBuffer = writeStreamBuffer(L, pMysqlParser, pStreamBuffer, pBuffer, nOffset);
        pMysqlParser->nStreamLength -= sizeof(uint32_t);
        const char* pBuf            = pStreamBuffer + pMysqlParser->nStreamLength;
        uint32_t    uiPayloadLength = ((uint32_t)((uint8_t)(pBuf[2])) << 16) +
                                   ((uint32_t)((uint8_t)(pBuf[1])) << 8) +
                                   (uint32_t)((uint8_t)(pBuf[0]));
        pMysqlParser->nPayloadLength += uiPayloadLength;
        if (nOffset == nLength) {
            lua_pushinteger(L, 0);
            return 1;
        }
    }

    size_t nNeedToWrite = pMysqlParser->nPayloadLength - pMysqlParser->nStreamLength;
    if (nNeedToWrite != 0) {
        if (nLength - nOffset < nNeedToWrite) {
            writeStreamBuffer(L, pMysqlParser, pStreamBuffer, pBuffer + nOffset, nLength - nOffset);
            lua_pushinteger(L, 0);
            return 1;
        }
        pStreamBuffer =
            writeStreamBuffer(L, pMysqlParser, pStreamBuffer, pBuffer + nOffset, nNeedToWrite);
        nOffset += nNeedToWrite;

        size_t nParserOffset = 0;
        do {
            size_t nRead = mysql_parser_execute(&pMysqlParser->parser,
                                                &s_mysqlParserSettings,
                                                pStreamBuffer + nParserOffset,
                                                pMysqlParser->nStreamLength - nParserOffset);
            if (MYSQL_PARSER_ERRNO(&pMysqlParser->parser) != mysql_errno_ok) {
                lua_pushinteger(L, -1);
                return 1;
            }

            if (nRead == 0) {
                break;
            }
            nParserOffset += nRead;
        } while (nParserOffset < pMysqlParser->nStreamLength);

        if (nParserOffset != pMysqlParser->nStreamLength) {
            memmove(pStreamBuffer,
                    pStreamBuffer + nParserOffset,
                    pMysqlParser->nStreamLength - nParserOffset);
        }

        pMysqlParser->nStreamLength -= nParserOffset;
        pMysqlParser->nPayloadLength -= nParserOffset;
    }

    while (nLength - nOffset >= sizeof(uint32_t)) {
        const char* pBuf           = pBuffer + nOffset;
        size_t      nPayloadLength = ((uint32_t)((uint8_t)(pBuf[2])) << 16) +
                                ((uint32_t)((uint8_t)(pBuf[1])) << 8) +
                                (uint32_t)((uint8_t)(pBuf[0]));
        if (nLength - nOffset - sizeof(uint32_t) < nPayloadLength) {
            pMysqlParser->nPayloadLength += nPayloadLength;
            nOffset += sizeof(uint32_t);
            break;
        }
        nOffset += sizeof(uint32_t);
        if (pMysqlParser->nStreamLength > 0) {
            pStreamBuffer = writeStreamBuffer(
                L, pMysqlParser, pStreamBuffer, pBuffer + nOffset, nPayloadLength);

            size_t nParserOffset = 0;
            do {
                size_t nRead = mysql_parser_execute(&pMysqlParser->parser,
                                                    &s_mysqlParserSettings,
                                                    pStreamBuffer + nParserOffset,
                                                    pMysqlParser->nStreamLength - nParserOffset);
                if (MYSQL_PARSER_ERRNO(&pMysqlParser->parser) != mysql_errno_ok) {
                    lua_pushinteger(L, -1);
                    return 1;
                }

                if (nRead == 0) {
                    break;
                }
                nParserOffset += nRead;
            } while (nParserOffset < pMysqlParser->nStreamLength);

            if (nParserOffset != pMysqlParser->nStreamLength) {
                memmove(pStreamBuffer,
                        pStreamBuffer + nParserOffset,
                        pMysqlParser->nStreamLength - nParserOffset);
            }
            pMysqlParser->nStreamLength -= nParserOffset;
            pMysqlParser->nPayloadLength -= nParserOffset;
        }
        else {
            size_t      nParserOffset = 0;
            const char* pParserBuffer = pBuffer + nOffset;
            do {
                size_t nRead = mysql_parser_execute(&pMysqlParser->parser,
                                                    &s_mysqlParserSettings,
                                                    pParserBuffer + nParserOffset,
                                                    nPayloadLength - nParserOffset);
                if (MYSQL_PARSER_ERRNO(&pMysqlParser->parser) != mysql_errno_ok) {
                    lua_pushinteger(L, -1);
                    return 1;
                }

                if (nRead == 0) {
                    break;
                }
                nParserOffset += nRead;
            } while (nParserOffset < nPayloadLength);

            if (nParserOffset != nPayloadLength) {
                pStreamBuffer = writeStreamBuffer(L,
                                                  pMysqlParser,
                                                  pStreamBuffer,
                                                  pParserBuffer + nParserOffset,
                                                  nPayloadLength - nParserOffset);
                pMysqlParser->nPayloadLength += (nPayloadLength - nParserOffset);
            }
        }
        nOffset += nPayloadLength;
    }

    if (nLength != nOffset) {
        writeStreamBuffer(L, pMysqlParser, pStreamBuffer, pBuffer + nOffset, nLength - nOffset);
    }
    lua_pushinteger(L, pMysqlParser->iCount - iCount);
    return 1;
}

static inline void lua_mysqlResult(struct lua_State* L, mysqlResult_tt* pResult, bool bArray)
{
    switch (pResult->type) {
    case mysql_result_type_handshake:
    {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "handshake");
        lua_setfield(L, -2, "type");
        lua_pushlstring(L, pResult->result, 20);
        lua_setfield(L, -2, "scramble");
    } break;
    case mysql_result_type_set:
    {
        char num[MYSQL_NUM_STR_LENGTH + 1];
        char float_num[MYSQL_FLOAT_STR_LENGTH + 1];
        char double_num[MYSQL_DOUBLE_STR_LENGTH + 1];

        resultTypeSet_tt* pResultSet = (resultTypeSet_tt*)pResult->result;

        lua_createtable(L, pResultSet->rowCount, 0);
        if (bArray) {
            luaL_checkstack(L, pResultSet->rowCount, NULL);
            row_tt* pRow = pResultSet->row;
            for (uint32_t i = 0; i < pResultSet->rowCount; i++) {
                lua_createtable(L, pResultSet->fieldCount, 0);
                luaL_checkstack(L, pResultSet->fieldCount, NULL);
                for (uint32_t j = 0; j < pResultSet->fieldCount; j++) {
                    switch (pResultSet->field[j].type) {
                    case mysql_column_type_tiny:
                    case mysql_column_type_short:
                    case mysql_column_type_long:
                    case mysql_column_type_int24:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(num, pRow->cell[j].value, pRow->cell[j].len);
                            num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, atoi(num));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_rawseti(L, -2, j + 1);
                    } break;
                    case mysql_column_type_longlong:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(num, pRow->cell[j].value, pRow->cell[j].len);
                            num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtoull(num, NULL, 10));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_rawseti(L, -2, j + 1);
                    } break;
                    case mysql_column_type_float:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(float_num, pRow->cell[j].value, pRow->cell[j].len);
                            float_num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtof(float_num, NULL));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_rawseti(L, -2, j + 1);
                    } break;
                    case mysql_column_type_double:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(double_num, pRow->cell[j].value, pRow->cell[j].len);
                            double_num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtof(double_num, NULL));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_rawseti(L, -2, j + 1);
                    } break;
                    default:
                    {
                        if (pRow->cell[j].len > 0) {
                            lua_pushlstring(L, pRow->cell[j].value, pRow->cell[j].len);
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_rawseti(L, -2, j + 1);
                    }
                    }
                }
                lua_rawseti(L, -2, i + 1);
                pRow = pRow->next;
            }
        }
        else {
            luaL_checkstack(L, pResultSet->rowCount, NULL);
            row_tt* pRow = pResultSet->row;
            for (uint32_t i = 0; i < pResultSet->rowCount; i++) {
                lua_createtable(L, 0, pResultSet->fieldCount);
                luaL_checkstack(L, pResultSet->fieldCount, NULL);
                for (uint32_t j = 0; j < pResultSet->fieldCount; j++) {
                    switch (pResultSet->field[j].type) {
                    case mysql_column_type_tiny:
                    case mysql_column_type_short:
                    case mysql_column_type_long:
                    case mysql_column_type_int24:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(num, pRow->cell[j].value, pRow->cell[j].len);
                            num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, atoi(num));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_setfield(L, -2, pResultSet->field[j].name);
                    } break;
                    case mysql_column_type_longlong:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(num, pRow->cell[j].value, pRow->cell[j].len);
                            num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtoull(num, NULL, 10));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_setfield(L, -2, pResultSet->field[j].name);
                    } break;
                    case mysql_column_type_float:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(float_num, pRow->cell[j].value, pRow->cell[j].len);
                            float_num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtof(float_num, NULL));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_setfield(L, -2, pResultSet->field[j].name);
                    } break;
                    case mysql_column_type_double:
                    {
                        if (pRow->cell[j].len > 0) {
                            memcpy(double_num, pRow->cell[j].value, pRow->cell[j].len);
                            double_num[pRow->cell[j].len] = '\0';
                            lua_pushinteger(L, strtof(double_num, NULL));
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_setfield(L, -2, pResultSet->field[j].name);
                    } break;
                    default:
                    {
                        if (pRow->cell[j].len > 0) {
                            lua_pushlstring(L, pRow->cell[j].value, pRow->cell[j].len);
                        }
                        else {
                            lua_pushnil(L);
                        }
                        lua_setfield(L, -2, pResultSet->field[j].name);
                    }
                    }
                }
                lua_rawseti(L, -2, i + 1);
                pRow = pRow->next;
            }
        }
    } break;
    case mysql_result_type_ok:
    {
        resultTypeOK_tt* pResultOk = (resultTypeOK_tt*)pResult->result;
        lua_createtable(L, 0, 4);
        lua_pushstring(L, "ok");
        lua_setfield(L, -2, "type");

        lua_pushinteger(L, pResultOk->affectedRows);
        lua_setfield(L, -2, "affectedRows");

        lua_pushinteger(L, pResultOk->insertId);
        lua_setfield(L, -2, "insertId");

        lua_pushinteger(L, pResultOk->warningCount);
        lua_setfield(L, -2, "warningCount");
    } break;
    case mysql_result_type_error:
    {
        resultTypeError_tt* pResultError = (resultTypeError_tt*)pResult->result;

        lua_createtable(L, 0, 4);

        lua_pushstring(L, "error");
        lua_setfield(L, -2, "type");

        lua_pushinteger(L, pResultError->errnoCode);
        lua_setfield(L, -2, "errno");

        lua_pushlstring(L, pResultError->errorMsg, pResultError->msgLength);
        lua_setfield(L, -2, "errorMsg");

        lua_pushlstring(L, pResultError->sqlState, MYSQL_STATE_LENGTH);
        lua_setfield(L, -2, "state");
    } break;
    }
}

static int32_t lread(struct lua_State* L)
{
    lmysqlParser_tt* pMysqlParser = lua_touserdata(L, 1);
    luaL_argcheck(L, pMysqlParser != NULL, 1, "invalid user data");
    if (pMysqlParser->iCount <= 0) {
        return 0;
    }
    bool bArray = (bool)lua_toboolean(L, 2);
    --pMysqlParser->iCount;

    int32_t iResultCount = 0;

    mysqlResult_tt* pResult = pMysqlParser->pResult;
    while (pResult) {
        ++iResultCount;
        if (pResult->bEof) {
            break;
        }
        pResult = pResult->next;
    }

    for (int32_t i = 0; i < iResultCount; i++) {
        mysqlResult_tt* pResult = pMysqlParser->pResult;
        lua_mysqlResult(L, pResult, bArray);
        if (pResult == pMysqlParser->pLastResult) {
            pMysqlParser->pLastResult = NULL;
            pMysqlParser->pResult     = NULL;
        }
        else {
            pMysqlParser->pResult = pResult->next;
            pResult->next         = NULL;
        }
        mysqlResult_free(pResult);
    }
    return iResultCount;
}

int32_t luaopen_lruntime_mysql(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_lmysql[] = {{"codecHandle", lhandle},
                                {"new", lnew},
                                {"reset", lreset},
                                {"write", lwrite},
                                {"read", lread},
                                {NULL, NULL}};
    luaL_newlib(L, lualib_lmysql);
    return 1;
}
