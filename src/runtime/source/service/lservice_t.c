

#include "service/lservice_t.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "log_t.h"
#include "thread_t.h"
#include "time_t.h"
#include "utility_t.h"

#include "channel/channelCenter_t.h"
#include "channel/channel_t.h"
#include "eventIO/eventIOThread_t.h"
#include "eventIO/eventIO_t.h"
#include "serviceCenter_t.h"
#include "serviceEvent_t.h"
#include "service_t.h"

#include "internal/lconfig_t.h"
#include "internal/lloadCache_t.h"
#include "internal/lpackagePath_t.h"

#include "internal/lconnector_t.h"
#include "internal/ldnsResolve_t.h"
#include "internal/lenv-inl.h"
#include "internal/llistenPort_t.h"
#include "internal/ltimerWatcher_t.h"

#ifndef MAX_PATH
#    define MAX_PATH 260
#endif

#define def_MAX_ERROR_STR 256

static int32_t traceback(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else {
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

typedef struct lserviceContext_s
{
    service_tt* pHandle;
    uint32_t    uiGenToken;
    lua_State*  pLuaState;
    FILE*       pLogFile;
    bool        bProfile;
    bool        bLog;
    int32_t     iLogCount;
    uint64_t    uiProfileCost;
    uint64_t    uiProfileTimer;
    uint64_t    uiCallbackCount;
} lserviceContext_tt;

static void* lua_custom_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    if (nsize == 0) {
        mem_free(ptr);
        return NULL;
    }
    else {
        return mem_realloc(ptr, nsize);
    }
}

static uint32_t lserviceContext_genToken(lserviceContext_tt* pService)
{
    ++pService->uiGenToken;
    if (pService->uiGenToken == 0xffffffff) {
        pService->uiGenToken = 1;
    }
    return pService->uiGenToken;
}

static bool lserviceContext_send(lserviceContext_tt* pService, uint32_t uiDestination,
                                 const void* pBuffer, int32_t iLength, uint32_t uiFlag,
                                 uint32_t uiToken)
{
    service_tt* pServiceHandle = serviceCenter_gain(uiDestination);
    if (pServiceHandle) {
        bool bSucc = service_send(pServiceHandle,
                                  service_getID(pService->pHandle),
                                  pBuffer,
                                  iLength,
                                  DEF_EVENT_MSG | uiFlag,
                                  uiToken);
        service_release(pServiceHandle);
        return bSucc;
    }
    return false;
}

static bool lserviceContext_redirect(uint32_t sourceID, uint32_t uiDestinationID,
                                     const void* pBuffer, size_t nLength, uint32_t uiFlag,
                                     uint32_t uiToken)
{
    service_tt* pServiceHandle = serviceCenter_gain(uiDestinationID);
    if (pServiceHandle) {
        bool bSucc = service_send(pServiceHandle, sourceID, pBuffer, nLength, uiFlag, uiToken);
        service_release(pServiceHandle);
        return bSucc;
    }
    return false;
}

static bool channelSend(uint32_t uiDestination, const void* pBuffer, int32_t iLength,
                        uint32_t uiFlag, uint32_t uiToken)
{
    channel_tt* pChannel = channelCenter_gain(uiDestination);
    if (pChannel) {
        bool bSucc = channel_send(pChannel, pBuffer, iLength, uiFlag, uiToken) >= 0;
        channel_release(pChannel);
        return bSucc;
    }
    return false;
}

static void lserviceContext_localPrint(lserviceContext_tt* pService, const char* eLevel,
                                       const char* szText, bool bConsole)
{
    if (pService->pLogFile == NULL) {
        size_t nLength = strlen(luaConfig_getLogPath());
        char   tmp[nLength + 16];
        sprintf(tmp,
                "%s/%s-%08x.log",
                luaConfig_getLogPath(),
                luaConfig_getBootstrapParam(),
                service_getID(pService->pHandle));
        pService->pLogFile = fopen(tmp, "ab");
    }

    ++pService->iLogCount;
    time_t SetTime;
    time(&SetTime);
    struct tm* pTm;
    pTm = localtime(&SetTime);
    pTm->tm_year += 1900;
    pTm->tm_mon += 1;

    if (eLevel == NULL || strlen(eLevel) == 0) {
        fprintf(pService->pLogFile,
                "[%s %d-%d-%d %d:%d:%d] logCount:[%d] enLogSeverityLevel is null !!!! \n",
                "FATAL",
                pTm->tm_year,
                pTm->tm_mon,
                pTm->tm_mday,
                pTm->tm_hour,
                pTm->tm_min,
                pTm->tm_sec,
                pService->iLogCount);
        fflush(pService->pLogFile);
        return;
    }

    enLogSeverityLevel ele = (enLogSeverityLevel)atoi(eLevel);
    if (elog_trace > ele || eLog_fatal < ele) {
        fprintf(pService->pLogFile,
                "[%s %d-%d-%d %d:%d:%d] logCount:[%d] enLogSeverityLevel error !!!! eLevel:%d\n",
                "FATAL",
                pTm->tm_year,
                pTm->tm_mon,
                pTm->tm_mday,
                pTm->tm_hour,
                pTm->tm_min,
                pTm->tm_sec,
                pService->iLogCount,
                ele);
        fflush(pService->pLogFile);
        return;
    }

    if (bConsole) {
        printf("%s[%s %d-%d-%d %d:%d:%d]%s %s\n",
               logErrorColorArray[ele],
               logErrorStrArray[ele],
               pTm->tm_year,
               pTm->tm_mon,
               pTm->tm_mday,
               pTm->tm_hour,
               pTm->tm_min,
               pTm->tm_sec,
               NONE,
               szText);
    }


    fprintf(pService->pLogFile,
            "[%s %d-%d-%d %d:%d:%d] logCount:[%d] %s\n",
            logErrorStrArray[ele],
            pTm->tm_year,
            pTm->tm_mon,
            pTm->tm_mday,
            pTm->tm_hour,
            pTm->tm_min,
            pTm->tm_sec,
            pService->iLogCount,
            szText);
    fflush(pService->pLogFile);
}

static void llog(lserviceContext_tt* pService, const char* szFmt, ...)
{
    char szErrstr[def_MAX_ERROR_STR];
    memset(szErrstr, 0, def_MAX_ERROR_STR);

    va_list args;
    va_start(args, szFmt);
    size_t nOutBufferLength = vsnprintf(szErrstr, def_MAX_ERROR_STR, szFmt, args);
    va_end(args);

    char* szErrBuffer = NULL;
    if (nOutBufferLength >= def_MAX_ERROR_STR) {
        size_t nMaxLength = def_MAX_ERROR_STR;
        while (true) {
            nMaxLength *= 2;

            szErrBuffer = (char*)mem_malloc(nMaxLength);
            memset(szErrBuffer, 0, nMaxLength);

            va_start(args, szFmt);
            nOutBufferLength = vsnprintf(szErrBuffer, nMaxLength, szFmt, args);
            va_end(args);
            if (nOutBufferLength < nMaxLength) {
                break;
            }

            mem_free(szErrBuffer);
        }
    }
    else {
        szErrBuffer = mem_strdup(szErrstr);
    }

    static uint32_t uiLogService = 0;
    if (uiLogService == 0) {
        uiLogService = serviceCenter_findServiceID(luaConfig_getLogService());
    }

    service_tt* pLogServiceHandle = serviceCenter_gain(uiLogService);
    if (pLogServiceHandle) {
        service_sendMove(pLogServiceHandle,
                         service_getID(pService->pHandle),
                         szErrBuffer,
                         nOutBufferLength,
                         DEF_EVENT_MSG | DEF_EVENT_MSG_TEXT,
                         0);
        service_release(pLogServiceHandle);
    }
    else {
        size_t nb;
        char** arr = str_split_count(szFmt, "$", &nb);
        lserviceContext_localPrint(pService, nb > 0 ? arr[0] : "", szErrBuffer, false);
        mem_free(szErrBuffer);
    }
}

static int32_t lserviceContext_callback(lserviceContext_tt* pService, int32_t iEvent,
                                        uint32_t uiSourceID, uint32_t uiToken, void* pBuffer,
                                        size_t nLength)
{
    lua_State* L    = pService->pLuaState;
    int32_t    iTop = lua_gettop(L);
    if (iTop == 0) {
        lua_pushcfunction(L, traceback);
        lua_rawgetp(L, LUA_REGISTRYINDEX, lserviceContext_callback);
    }
    else {
        assert(iTop == 2);
    }
    lua_pushvalue(L, 2);
    lua_pushinteger(L, iEvent);
    lua_pushinteger(L, uiSourceID);
    lua_pushinteger(L, uiToken);
    lua_pushlightuserdata(L, pBuffer);
    lua_pushinteger(L, nLength);

    int32_t iRet = lua_pcall(L, 5, 0, 1);
    if (iRet == LUA_OK) {
        return 0;
    }

    switch (iRet) {
    case LUA_ERRRUN:
    {
        llog(pService,
             "%d$lua call [type:%d source:%x8 token:%d] to Lua API error : %s",
             eLog_error,
             iEvent,
             uiSourceID,
             uiToken,
             lua_tostring(L, -1));
    } break;
    case LUA_ERRMEM:
    {
        llog(pService,
             "%d$lua memory [type:%d source:%x8 token:%d]",
             eLog_error,
             iEvent,
             uiSourceID,
             uiToken);
    } break;
    case LUA_ERRERR:
    {
        llog(pService,
             "%d$lua error [type:%d source:%x8 token:%d]",
             eLog_error,
             iEvent,
             uiSourceID,
             uiToken);
    } break;
    };

    lua_pop(L, 1);
    return 0;
}

static bool service_callback(int32_t iType, uint32_t uiSourceID, uint32_t uiToken, void* pBuffer,
                             size_t nLength, void* pUserData)
{
    if (pUserData == NULL) {
        return false;
    }
    lserviceContext_tt* pService = (lserviceContext_tt*)pUserData;
    ++pService->uiCallbackCount;
    if (pService->bProfile) {
        pService->uiProfileTimer = getThreadClock();
        lserviceContext_callback(pService, iType, uiSourceID, uiToken, pBuffer, nLength);
        uint64_t uiTimer = getThreadClock() - pService->uiProfileTimer;
        pService->uiProfileCost += uiTimer;
    }
    else {
        lserviceContext_callback(pService, iType, uiSourceID, uiToken, pBuffer, nLength);
    }
    return true;
}

 const char *lua_dofunction = "function frog_addLuaState()\n"
        "local dbg = require('frog_debug')\n"
        "dbg.startDebugServer('%s', %d)\n"
        "dbg.addLuaState()\n"
        "end"
        "";


static void addLuaState(lua_State* L, const char* debug_ip, const char* debug_port)
{
    if (NULL == debug_ip) {
        return;
    }

    if (NULL == debug_port) {
        return;
    }

    int port = strtol(debug_port, NULL, 10);

    char loadstr[200];
    sprintf(loadstr, lua_dofunction, debug_ip, port);

    int oldn   = lua_gettop(L);
    int status = luaL_dostring(L, loadstr);
    if (status != 0) {
        const char* ret = lua_tostring(L, -1);
        lua_settop(L, oldn);            
        Log(eLog_error, "[ERROR] addLuaState lua_tostring error!! err:%s", ret);
        return;
    }

    lua_getglobal(L, "frog_addLuaState");
    if (!lua_isfunction(L, -1)) {
        const char* ret = lua_tostring(L, -1);
        lua_settop(L, oldn);
        Log(eLog_error, "[ERROR] addLuaState lua_getglobal addLuaState error!! err:%s", ret);
        return;
    }

    status = lua_pcall(L, 0, 0, 0);
    if (status != 0) {
        const char* ret = lua_tostring(L, -1);
        lua_settop(L, oldn);
        Log(eLog_error, "[ERROR] addLuaState lua_pcall addLuaState error!! err:%s", ret);
        return;
    }
}


static bool service_startCallback(void* pUserData)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)pUserData;
    if (lua_pcall(pService->pLuaState, 1, 0, 1) != LUA_OK) {
        Log(eLog_error,
            "PANIC: unprotected error in call to Lua API error:%s",
            lua_tostring(pService->pLuaState, -1));
        return false;
    }

    lua_pop(pService->pLuaState, 1);
    lua_gc(pService->pLuaState, LUA_GCRESTART, 0);
    return true;
}

static void service_stopCallback(void* pUserData)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)pUserData;
    if (pService->pLogFile) {
        fclose(pService->pLogFile);
        pService->pLogFile = NULL;
    }

    if (pService->pLuaState) {
        lua_close(pService->pLuaState);
        pService->pLuaState = NULL;
    }

    if (pService->pHandle) {
        service_release(pService->pHandle);
        pService->pHandle = NULL;
    }

    mem_free(pService);
}

uint32_t createLuaService(const char* szName, const char* pParam, size_t nLength)
{
    eventIO_tt* pEventIO = getEnvEventIO();

    char szLoaderFile[MAX_PATH];
    bzero(szLoaderFile, MAX_PATH);
    const char* szLoaderPath    = luaConfig_getServicePath();
    size_t      nLoadFileLength = strlen(szLoaderPath);
    strcpy(szLoaderFile, szLoaderPath);
    strcpy(szLoaderFile + nLoadFileLength, "/");
    ++nLoadFileLength;
    strcpy(szLoaderFile + nLoadFileLength, szName);
    nLoadFileLength += strlen(szName);
    strcpy(szLoaderFile + nLoadFileLength, ".lua");

    lserviceContext_tt* pServiceL = mem_malloc(sizeof(lserviceContext_tt));
    pServiceL->pHandle            = NULL;
    pServiceL->pLuaState          = NULL;
    pServiceL->uiGenToken         = 0;
    pServiceL->pLogFile           = NULL;
    pServiceL->iLogCount          = 0;
    pServiceL->bLog               = luaConfig_isLog();
    pServiceL->bProfile           = luaConfig_isProfile();
    pServiceL->uiProfileCost      = 0;
    pServiceL->uiProfileTimer     = 0;
    pServiceL->uiCallbackCount    = 0;

    lua_State* pLuaState = lua_newstate(lua_custom_alloc, NULL);
    lua_gc(pLuaState, LUA_GCSTOP, 0);
    pServiceL->pLuaState = pLuaState;
    pServiceL->pHandle   = createService(pEventIO);
    service_setCallback(pServiceL->pHandle, service_callback);

    // lua_atpanic(pLuaState, &lua_panic);
    luaL_openlibs(pLuaState);
    setPackage_path(pLuaState, luaConfig_getLoaderPath());
#if defined(_WINDOWS) || defined(_WIN32)
    setPackage_cpath(pLuaState, "modules/?.dll");
#elif defined(__APPLE__)
    setPackage_cpath(pLuaState, "modules/?.dylib");
#elif defined(__linux__)
    setPackage_cpath(pLuaState, "modules/?.so");
#endif
    lua_pushlightuserdata(pLuaState, pServiceL);
    lua_setfield(pLuaState, LUA_REGISTRYINDEX, "service_context");

    lua_pushcfunction(pLuaState, traceback);
    Check(lua_gettop(pLuaState) == 1);

    // lua loadcache
    lua_getfield(pLuaState, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    lua_getglobal(pLuaState, "package");
    lua_getfield(pLuaState, -1, "searchers");
    lua_pushvalue(pLuaState, -2);
    lua_pushcclosure(pLuaState, loadCache, 1);
    for (size_t i = lua_rawlen(pLuaState, -2) + 1; i > 2; --i) {
        lua_rawgeti(pLuaState, -2, i - 1);
        lua_rawseti(pLuaState, -3, i);
    }
    lua_rawseti(pLuaState, -2, 2);
    lua_setfield(pLuaState, -2, "loaders");
    lua_pop(pLuaState, 2);

    if (loadfileCache(pLuaState, szLoaderFile) != LUA_OK) {
        Log(eLog_error, "load bootstrap error:%s", lua_tostring(pLuaState, -1));
        return 0;
    }
    if (pParam) {
        lua_pushlstring(pLuaState, pParam, nLength);
    }
    else {
        lua_pushnil(pLuaState);
    }

    addLuaState(pLuaState, luaConfig_getDebug_ip(), luaConfig_getDebug_port());
    return service_start(
        pServiceL->pHandle, pServiceL, service_startCallback, service_stopCallback);
}

static int32_t lservice_context_genToken(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    uint32_t            uiToken  = lserviceContext_genToken(pService);
    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_send(lua_State* L)
{
    lserviceContext_tt* pService      = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    int32_t             isNum         = 0;
    uint32_t            uiDestination = (uint32_t)lua_tointegerx(L, 1, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 1);
        if (szDestinationName) {
            uiDestination = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestination = 0;
        }
    }

    if (uiDestination == 0) {
        return 0;
    }

    uint32_t uiEvent = (uint32_t)luaL_checkinteger(L, 2);

    uint32_t uiToken = 0;
    if (lua_isnoneornil(L, 3)) {
        uiToken = lserviceContext_genToken(pService);
    }
    else {
        uiToken = luaL_checkinteger(L, 3);
    }

    const char* pBuffer;
    size_t      nLength = 0;

    int32_t iMsgInputType = lua_type(L, 4);
    switch (iMsgInputType) {
    case LUA_TSTRING:
    {
        pBuffer = lua_tolstring(L, 4, &nLength);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        pBuffer = (const char*)lua_touserdata(L, 4);
        nLength = luaL_checkinteger(L, 5);
    } break;
    default: luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, 4)));
    }

    if (nLength > 0xFFFFFF) {
        llog(pService,
             "%d$send length > 15M [event:%d source:%x8 destination:%x8]",
             eLog_error,
             uiEvent,
             service_getID(pService->pHandle),
             uiDestination);
        return 0;
    }

    if (uiDestination & 0x80000000) {
        if (!channelSend(uiDestination, pBuffer, (int32_t)nLength, uiEvent, uiToken)) {
            return 0;
        }
    }
    else {
        if (!lserviceContext_send(
                pService, uiDestination, pBuffer, (int32_t)nLength, uiEvent, uiToken)) {
            return 0;
        }
    }
    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_command(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t  isNum         = 0;
    uint32_t uiDestination = (uint32_t)lua_tointegerx(L, 1, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 1);
        if (szDestinationName) {
            uiDestination = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestination = 0;
        }
    }

    if (uiDestination == 0 || uiDestination & 0x80000000) {
        return 0;
    }

    uint32_t uiToken = 0;
    if (lua_isnoneornil(L, 2)) {
        uiToken = lserviceContext_genToken(pService);
    }
    else {
        uiToken = luaL_checkinteger(L, 2);
    }

    const char* pBuffer;
    size_t      nLength = 0;

    int32_t iMsgInputType = lua_type(L, 3);
    switch (iMsgInputType) {
    case LUA_TSTRING:
    {
        pBuffer = lua_tolstring(L, 3, &nLength);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        pBuffer = (const char*)lua_touserdata(L, 3);
        nLength = luaL_checkinteger(L, 4);
    } break;
    default: luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, 3)));
    }

    if (nLength > 0xFFFFFF) {
        llog(pService,
             "%d$command length > 15M [source:%x8 destination:%x8]",
             eLog_error,
             service_getID(pService->pHandle),
             uiDestination);
        return 0;
    }

    if (!lserviceContext_send(
            pService, uiDestination, pBuffer, nLength, DEF_EVENT_COMMAND, uiToken)) {
        return 0;
    }
    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_pong(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t  isNum         = 0;
    uint32_t uiDestination = (uint32_t)lua_tointegerx(L, 1, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 1);
        if (szDestinationName) {
            uiDestination = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestination = 0;
        }
    }

    if (uiDestination == 0) {
        return 0;
    }

    uint32_t uiToken = luaL_checkinteger(L, 2);

    if (uiDestination & 0x80000000) {
        if (!channelSend(uiDestination, NULL, 0, DEF_EVENT_MSG_PONG, uiToken)) {
            return 0;
        }
    }
    else {
        if (!lserviceContext_send(pService, uiDestination, NULL, 0, DEF_EVENT_MSG_PONG, uiToken)) {
            return 0;
        }
    }
    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_ping(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t  isNum         = 0;
    uint32_t uiDestination = (uint32_t)lua_tointegerx(L, 1, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 1);
        if (szDestinationName) {
            uiDestination = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestination = 0;
        }
    }

    if (uiDestination == 0) {
        return 0;
    }

    uint32_t uiToken = lserviceContext_genToken(pService);

    if (uiDestination & 0x80000000) {
        if (!channelSend(uiDestination, NULL, 0, DEF_EVENT_MSG_PING, uiToken)) {
            return 0;
        }
    }
    else {
        if (!lserviceContext_send(pService, uiDestination, NULL, 0, DEF_EVENT_MSG_PING, uiToken)) {
            return 0;
        }
    }

    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_sendClose(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t  isNum         = 0;
    uint32_t uiDestination = (uint32_t)lua_tointegerx(L, 1, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 1);
        if (szDestinationName) {
            uiDestination = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestination = 0;
        }
    }

    if (uiDestination == 0) {
        return 0;
    }

    uint32_t uiToken = 0;
    if (!lua_isinteger(L, 2)) {
        uiToken = lua_tointeger(L, 2);
    }

    if (uiDestination & 0x80000000) {
        if (!channelSend(uiDestination, NULL, 0, DEF_EVENT_MSG_CLOSE, uiToken)) {
            return 0;
        }
    }
    else {
        if (!lserviceContext_send(pService, uiDestination, NULL, 0, DEF_EVENT_MSG_CLOSE, uiToken)) {
            return 0;
        }
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lservice_context_yield(lua_State* L)
{
    lserviceContext_tt* pService      = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    uint32_t            uiDestination = service_getID(pService->pHandle);

    uint32_t uiToken = lserviceContext_genToken(pService);
    if (service_send(pService->pHandle, uiDestination, NULL, 0, DEF_EVENT_YIELD, uiToken)) {
        lua_pushinteger(L, uiToken);
        return 1;
    }
    return 0;
}

static int32_t lservice_context_timeout(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t iCount = lua_gettop(L);
    if (iCount < 1) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        return 0;
    }

    uint64_t uiIntervalMs = lua_tointeger(L, 1);

    uint32_t uiToken = lserviceContext_genToken(pService);

    timerWatcher_tt* pTimerWatcherHandle = createTimerWatcher(pService->pHandle, uiToken);

    timerWatcher_start(pTimerWatcherHandle, true, uiIntervalMs);
    timerWatcher_release(pTimerWatcherHandle);

    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_runAfter(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t iCount = lua_gettop(L);
    if (iCount < 1) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TNUMBER) {
        return 0;
    }

    uint64_t uiIntervalMs = lua_tointeger(L, 1);

    uint32_t uiToken = lserviceContext_genToken(pService);

    ltimerWatcher_tt* pTimerWatcherL =
        (ltimerWatcher_tt*)lua_newuserdatauv(L, sizeof(ltimerWatcher_tt), 0);
    pTimerWatcherL->pHandle = createTimerWatcher(pService->pHandle, uiToken);

    timerWatcher_start(pTimerWatcherL->pHandle, true, uiIntervalMs);

    luaL_getmetatable(L, "timerWatcher");
    lua_setmetatable(L, -2);

    lua_pushinteger(L, uiToken);
    return 2;
}

static int32_t lservice_context_runEvery(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    int32_t iCount = lua_gettop(L);
    if (iCount < 2) {
        return 0;
    }

    if ((lua_type(L, 1) != LUA_TNUMBER) || (lua_type(L, 2) != LUA_TNUMBER)) {
        return 0;
    }

    uint64_t uiIntervalMs = lua_tointeger(L, 1);
    int32_t  iToken       = lua_tointeger(L, 2);

    ltimerWatcher_tt* pTimerWatcherL =
        (ltimerWatcher_tt*)lua_newuserdatauv(L, sizeof(ltimerWatcher_tt), 0);
    pTimerWatcherL->pHandle = createTimerWatcher(pService->pHandle, iToken);
    timerWatcher_start(pTimerWatcherL->pHandle, false, uiIntervalMs);
    luaL_getmetatable(L, "timerWatcher");
    lua_setmetatable(L, -2);
    return 1;
}

static int32_t lservice_context_self(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle) {
        lua_pushinteger(L, service_getID(pService->pHandle));
        return 1;
    }
    return 0;
}

static int32_t lservice_context_status(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle) {
        lua_pushinteger(L, pService->uiProfileCost / 1000000);
        lua_pushinteger(L, pService->uiCallbackCount);
        lua_pushinteger(L, service_queueSize(pService->pHandle));
    }
    else {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }
    return 3;
}

static int32_t lservice_context_exit(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle) {
        service_stop(pService->pHandle);
    }
    return 0;
}

static int32_t lservice_context_bindName(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle == NULL) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int32_t iType = lua_type(L, 1);
    if (iType != LUA_TSTRING) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char* szName = luaL_checkstring(L, 1);

    if (serviceCenter_bindName(service_getID(pService->pHandle), szName)) {
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int32_t lservice_context_listenPort(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle == NULL) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        return 0;
    }

    size_t      sz  = 0;
    const char* str = lua_tolstring(L, 1, &sz);

    bool bUDP = lua_toboolean(L, 2);

    listenPort_tt* pListenPortHandle = createListenPort(pService->pHandle);
    if (!listenPort_start(pListenPortHandle, str, !bUDP)) {
        listenPort_release(pListenPortHandle);
        return 0;
    }

    llistenPort_tt* pListenPort = (llistenPort_tt*)lua_newuserdatauv(L, sizeof(llistenPort_tt), 0);
    pListenPort->pHandle        = pListenPortHandle;
    luaL_getmetatable(L, "listenPort");
    lua_setmetatable(L, -2);
    return 1;
}

static int32_t lservice_context_connect(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle == NULL) {
        return 0;
    }

    if ((lua_type(L, 1) != LUA_TSTRING) || (lua_type(L, 2) != LUA_TNUMBER)) {
        return 0;
    }

    size_t      sz  = 0;
    const char* str = lua_tolstring(L, 1, &sz);

    uint64_t uiConnectingTimeoutMs = lua_tointeger(L, 2);

    bool bUDP = lua_toboolean(L, 3);

    uint32_t uiToken = lserviceContext_genToken(pService);

    connector_tt* pHandle = createConnector(pService->pHandle, uiToken);

    if (!connector_connect(pHandle, str, uiConnectingTimeoutMs, !bUDP)) {
        connector_release(pHandle);
        return 0;
    }
    lconnector_tt* pConnector = (lconnector_tt*)lua_newuserdatauv(L, sizeof(lconnector_tt), 0);
    pConnector->pHandle       = pHandle;
    luaL_getmetatable(L, "connector");
    lua_setmetatable(L, -2);
    lua_pushinteger(L, uiToken);
    return 2;
}

static int32_t lservice_context_localPrint(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    const char* eLevel   = luaL_checkstring(L, 1);
    const char* s        = luaL_checkstring(L, 2);
    bool        bConsole = (bool)lua_toboolean(L, 3);

    lserviceContext_localPrint(pService, eLevel, s, bConsole);
    return 0;
}

static int32_t lservice_context_remoteWrite(lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    uint32_t uiDestination = (uint32_t)lua_tointeger(L, 1);

    if (uiDestination == 0 || !(uiDestination & 0x80000000)) {
        return 0;
    }

    const char* pBuffer;
    size_t      nLength = 0;

    int32_t iMsgInputType = lua_type(L, 2);
    switch (iMsgInputType) {
    case LUA_TSTRING:
    {
        pBuffer = lua_tolstring(L, 2, &nLength);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        pBuffer = (char*)lua_touserdata(L, 2);
        nLength = luaL_checkinteger(L, 3);
    } break;
    default: luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, 2)));
    }

    if (nLength == 0) {
        return 0;
    }

    if (nLength > 0xFFFFFF) {
        llog(pService,
             "%d$remoteWrite length > 15M [source:%x8 destination:%x8]",
             eLog_error,
             service_getID(pService->pHandle),
             uiDestination);
        return 0;
    }

    channel_tt* pChannel = channelCenter_gain(uiDestination);
    if (pChannel) {
        if (channel_write(pChannel, pBuffer, nLength, 0) < 0) {
            channel_release(pChannel);
            return 0;
        }
        channel_release(pChannel);
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lservice_context_remoteWriteReq(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));

    uint32_t uiDestination = (uint32_t)lua_tointeger(L, 1);

    if (uiDestination == 0 || !(uiDestination & 0x80000000)) {
        return 0;
    }

    uint32_t uiToken = lserviceContext_genToken(pService);

    const char* pBuffer;
    size_t      nLength = 0;

    int32_t iMsgInputType = lua_type(L, 2);
    switch (iMsgInputType) {
    case LUA_TSTRING:
    {
        pBuffer = lua_tolstring(L, 2, &nLength);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        pBuffer = (const char*)lua_touserdata(L, 2);
        nLength = luaL_checkinteger(L, 3);
    } break;
    default: luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, 2)));
    }

    if (nLength == 0) {
        return 0;
    }

    if (nLength > 0xFFFFFF) {
        llog(pService,
             "%d$remoteWriteReq length > 15M [source:%x8 destination:%x8]",
             eLog_error,
             service_getID(pService->pHandle),
             uiDestination);
        return 0;
    }

    channel_tt* pChannel = channelCenter_gain(uiDestination);
    if (pChannel) {
        if (channel_write(pChannel, pBuffer, nLength, uiToken) < 0) {
            channel_release(pChannel);
            return 0;
        }
        channel_release(pChannel);
    }

    lua_pushinteger(L, uiToken);
    return 1;
}

static int32_t lservice_context_remoteBind(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (lua_type(L, 1) != LUA_TNUMBER) {
        return 0;
    }

    uint32_t    uiID     = (uint32_t)lua_tointeger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }

    bool bKeepAlive  = lua_toboolean(L, 2) ? true : false;
    bool bTcpNoDelay = lua_toboolean(L, 3) ? true : false;

    struct codecStream_s* pCodecHandle = NULL;

    if (lua_isuserdata(L, 4)) {
        pCodecHandle = lua_touserdata(L, 4);
    }
    channel_setCodecStream(pChannel, pCodecHandle);

    channel_bind(pChannel, pService->pHandle, bKeepAlive, bTcpNoDelay);
    channel_release(pChannel);
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lservice_context_dnsResolve(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (pService->pHandle == NULL) {
        return 0;
    }

    if (lua_type(L, 1) != LUA_TSTRING) {
        return 0;
    }

    size_t      sz  = 0;
    const char* str = lua_tolstring(L, 1, &sz);

    bool bLookHostsFile = lua_toboolean(L, 2) ? true : false;

    bool bIPv6 = lua_toboolean(L, 3) ? true : false;

    uint32_t uiToken = lserviceContext_genToken(pService);

    dnsResolve_tt* pHandle = createDnsResolve(pService->pHandle, bLookHostsFile);
    if (!dnsResolve_query(pHandle, str, bIPv6, 5000, uiToken)) {
        dnsResolve_release(pHandle);
        return 0;
    }

    ldnsResolve_tt* pDnsResolve = (ldnsResolve_tt*)lua_newuserdatauv(L, sizeof(ldnsResolve_tt), 0);
    pDnsResolve->pHandle        = pHandle;
    luaL_getmetatable(L, "dnsResolve");
    lua_setmetatable(L, -2);

    lua_pushinteger(L, uiToken);
    return 2;
}

static int32_t lservice_context_setProfile(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    pService->bProfile           = lua_toboolean(L, 1) ? true : false;
    return 0;
}

static int32_t lservice_context_setLog(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    pService->bLog               = lua_toboolean(L, 1) ? true : false;
    return 0;
}

static int32_t lservice_context_log(struct lua_State* L)
{
    lserviceContext_tt* pService = (lserviceContext_tt*)lua_touserdata(L, lua_upvalueindex(1));
    if (!pService->bLog) {
        return 0;
    }

    const char* s = luaL_checkstring(L, 1);

    llog(pService, s);
    return 0;
}

static int32_t lservice_create(lua_State* L)
{
    if (lua_isnoneornil(L, 2)) {
        lua_pushinteger(L, createLuaService(lua_tostring(L, 1), NULL, 0));
    }
    else {
        size_t      nLength = 0;
        const char* pParam  = lua_tolstring(L, 2, &nLength);
        lua_pushinteger(L, createLuaService(lua_tostring(L, 1), pParam, nLength));
    }
    return 1;
}

static int32_t lservice_find(lua_State* L)
{
    uint32_t uiDestination = 0;

    int32_t iType = lua_type(L, 1);
    if (iType != LUA_TSTRING) {
        return 0;
    }

    const char* szDestinationName = lua_tostring(L, 1);
    if (szDestinationName) {
        uiDestination = serviceCenter_findServiceID(szDestinationName);
    }
    else {
        uiDestination = 0;
    }

    if (uiDestination == 0) {
        return 0;
    }

    lua_pushinteger(L, uiDestination);
    return 1;
}

int32_t lservice_bindName(lua_State* L)
{
    uint32_t    uiServiceID = luaL_checkinteger(L, 1);
    const char* szName      = luaL_checkstring(L, 2);

    if (serviceCenter_bindName(uiServiceID, szName)) {
        lua_pushboolean(L, 1);
    }
    else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

int32_t lservice_unbindName(lua_State* L)
{
    int32_t iCount = lua_gettop(L);
    if (iCount != 1) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uint32_t uiDestination = 0;

    int32_t iType = lua_type(L, 1);
    if (iType != LUA_TSTRING) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char* szDestinationName = lua_tostring(L, 1);
    if (szDestinationName) {
        uiDestination = serviceCenter_findServiceID(szDestinationName);
    }
    else {
        uiDestination = 0;
    }

    if (uiDestination == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (serviceCenter_unbindName(uiDestination)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lservice_remoteClose(struct lua_State* L)
{
    uint32_t    uiID     = (uint32_t)luaL_checkinteger(L, 1);
    channel_tt* pChannel = channelCenter_gain(uiID);
    if (pChannel == NULL) {
        return 0;
    }
    uint64_t uiDisconnectTimeoutMs = (uint64_t)lua_tointeger(L, 2);
    channel_close(pChannel, uiDisconnectTimeoutMs);
    channel_release(pChannel);
    channel_release(pChannel);
    lua_pushboolean(L, 1);
    return 1;
}

static int32_t lservice_setCallback(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, lserviceContext_callback);
    return 0;
}

static int32_t lservice_cbufferToString(lua_State* L)
{
    if (lua_isnoneornil(L, 1)) {
        return 0;
    }
    const char* pBuffer = (char*)lua_touserdata(L, 1);
    lua_Integer sz      = luaL_checkinteger(L, 2);
    lua_pushlstring(L, pBuffer, (size_t)sz);
    return 1;
}

static int32_t lservice_hardwareConcurrency(struct lua_State* L)
{
    lua_pushinteger(L, threadHardwareConcurrency());
    return 1;
}

static int32_t lservice_getClockMonotonic(struct lua_State* L)
{
    timespec_tt t;
    getClockMonotonic(&t);
    uint64_t uiTime = timespec_toMsec(&t);
    lua_pushinteger(L, uiTime);
    return 1;
}

static int32_t lservice_getClockRealtime(struct lua_State* L)
{
    timespec_tt t;
    getClockRealtime(&t);
    uint64_t uiTime = timespec_toMsec(&t);
    lua_pushinteger(L, uiTime);
    return 1;
}

static int32_t lservice_redirect(lua_State* L)
{
    uint32_t uiSourceID = (uint32_t)luaL_checkinteger(L, 1);

    int32_t  isNum           = 0;
    uint32_t uiDestinationID = (uint32_t)lua_tointegerx(L, 2, &isNum);
    if (isNum == 0) {
        const char* szDestinationName = lua_tostring(L, 2);
        if (szDestinationName) {
            uiDestinationID = serviceCenter_findServiceID(szDestinationName);
        }
        else {
            uiDestinationID = 0;
        }
    }

    if (uiDestinationID == 0 || uiDestinationID & 0x80000000) {
        return 0;
    }

    uint32_t uiEventType = (uint32_t)luaL_checkinteger(L, 3);

    uint32_t uiToken = 0;
    if (!lua_isnoneornil(L, 4)) {
        uiToken = luaL_checkinteger(L, 4);
    }

    uint32_t uiEvent = 0;

    switch (uiEventType) {
    case 1:
    {
        uiEvent = DEF_EVENT_MSG | DEF_EVENT_MSG_CALL;
    } break;
    case 2:
    {
        uiEvent = DEF_EVENT_MSG | DEF_EVENT_MSG_PING;
    } break;
    case 3:
    {
        uiEvent = DEF_EVENT_MSG | DEF_EVENT_MSG_CLOSE;
    } break;
    case 4:
    {
        uiEvent = DEF_EVENT_MSG | DEF_EVENT_MSG_SEND;
    } break;
    case 5:
    {
        uiEvent = DEF_EVENT_MSG | DEF_EVENT_MSG_TEXT;
    } break;
    case 6:
    case 7:
    case 8:
    case 9:
    {
        uiEvent = uiEventType;
    } break;
    default:
    {
        return 0;
    }
    }

    const char* pBuffer;
    size_t      nLength = 0;

    int32_t iMsgInputType = lua_type(L, 5);
    switch (iMsgInputType) {
    case LUA_TSTRING:
    {
        pBuffer = lua_tolstring(L, 5, &nLength);
    } break;
    case LUA_TLIGHTUSERDATA:
    {
        pBuffer = (const char*)lua_touserdata(L, 5);
        nLength = luaL_checkinteger(L, 6);
    } break;
    default: luaL_error(L, "invalid param %s", lua_typename(L, lua_type(L, 5)));
    }

    if (!lserviceContext_redirect(
            uiSourceID, uiDestinationID, pBuffer, nLength, uiEvent, uiToken)) {
        return 0;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int32_t luaopen_lruntime_service(lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    registerTimerWatcherL(L);
    registerListenPortL(L);
    registerConnectorL(L);
    registerDnsResolveL(L);

    luaL_Reg lualib_service[] = {{"createService", lservice_create},
                                 {"findService", lservice_find},
                                 {"bindServiceName", lservice_bindName},
                                 {"unbindServiceName", lservice_unbindName},
                                 {"remoteClose", lservice_remoteClose},
                                 {"setCallback", lservice_setCallback},
                                 {"cbufferToString", lservice_cbufferToString},
                                 {"hardwareConcurrency", lservice_hardwareConcurrency},
                                 {"getClockMonotonic", lservice_getClockMonotonic},
                                 {"getClockRealtime", lservice_getClockRealtime},
                                 {"redirect", lservice_redirect},
                                 {NULL, NULL}};

    luaL_Reg lualib_service_context[] = {{"yield", lservice_context_yield},
                                         {"command", lservice_context_command},
                                         {"send", lservice_context_send},
                                         {"pong", lservice_context_pong},
                                         {"ping", lservice_context_ping},
                                         {"sendClose", lservice_context_sendClose},
                                         {"genToken", lservice_context_genToken},
                                         {"timeout", lservice_context_timeout},
                                         {"runAfter", lservice_context_runAfter},
                                         {"runEvery", lservice_context_runEvery},
                                         {"bindName", lservice_context_bindName},
                                         {"localPrint", lservice_context_localPrint},
                                         {"remoteWrite", lservice_context_remoteWrite},
                                         {"remoteWriteReq", lservice_context_remoteWriteReq},
                                         {"remoteBind", lservice_context_remoteBind},
                                         {"listenPort", lservice_context_listenPort},
                                         {"connect", lservice_context_connect},
                                         {"dnsResolve", lservice_context_dnsResolve},
                                         {"setProfile", lservice_context_setProfile},
                                         {"log", lservice_context_log},
                                         {"setLog", lservice_context_setLog},
                                         {"self", lservice_context_self},
                                         {"status", lservice_context_status},
                                         {"exit", lservice_context_exit},
                                         {NULL, NULL}};

    lua_createtable(L,
                    0,
                    sizeof(lualib_service) / sizeof(lualib_service[0]) +
                        sizeof(lualib_service_context) / sizeof(lualib_service_context[0]) - 2);

    lua_getfield(L, LUA_REGISTRYINDEX, "service_context");

    lserviceContext_tt* pContext = (lserviceContext_tt*)lua_touserdata(L, -1);
    if (pContext == NULL) {
        return luaL_error(L, "service_context");
    }

    luaL_setfuncs(L, lualib_service_context, 1);
    luaL_setfuncs(L, lualib_service, 0);
    return 1;
}
