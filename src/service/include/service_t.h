

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform_t.h"
#include "serviceEvent_t.h"

#if DEF_PLATFORM == DEF_PLATFORM_WINDOWS
#    ifdef _DEF_SERVICE_DLLEXPORT
#        define frService_API __declspec(dllexport)
#    else
#        define frService_API __declspec(dllimport)
#    endif
#else
#    ifdef _DEF_SERVICE_DLLEXPORT
#        define frService_API __attribute__((__visibility__("default")))
#    else
#        define frService_API extern
#    endif
#endif

struct eventIO_s;

struct service_s;
struct connector_s;
struct listenPort_s;
struct timerWatcher_s;
struct dnsResolve_s;

typedef struct service_s      service_tt;
typedef struct connector_s    connector_tt;
typedef struct dnsResolve_s   dnsResolve_tt;
typedef struct listenPort_s   listenPort_tt;
typedef struct timerWatcher_s timerWatcher_tt;

frService_API service_tt* createService(struct eventIO_s* pEventIO);

frService_API void service_setCallback(service_tt* pService, bool (*fn)(int32_t, uint32_t, uint32_t,
                                                                        void*, size_t, void*));

frService_API void service_addref(service_tt* pService);

frService_API void service_release(service_tt* pService);

frService_API uint32_t service_start(service_tt* pService, void* pUserData, bool (*fnStart)(void*),
                                     void (*fnStop)(void*));

frService_API void service_stop(service_tt* pService);

frService_API bool service_sendMove(service_tt* pService, uint32_t uiSourceID, void* pData,
                                    int32_t iLength, uint32_t uiFlag, uint32_t uiToken);

frService_API bool service_send(service_tt* pService, uint32_t uiSourceID, const void* pData,
                                int32_t iLength, uint32_t uiFlag, uint32_t uiToken);

frService_API uint32_t service_queueSize(service_tt* pService);

frService_API uint32_t service_getID(service_tt* pService);

frService_API struct eventIO_s* service_getEventIO(service_tt* pService);

// connector
frService_API connector_tt* createConnector(service_tt* pService, uint32_t uiToken);

frService_API void connector_addref(connector_tt* pHandle);

frService_API void connector_release(connector_tt* pHandle);

frService_API bool connector_connect(connector_tt* pHandle, const char* szAddress,
                                     int32_t iConnectingTimeoutMs, bool bTcp);

frService_API void connector_close(connector_tt* pHandle);

frService_API service_tt* connector_getService(connector_tt* pHandle);

// dnsResolve
frService_API void dnsStartup();

frService_API void dnsCleanup();

frService_API dnsResolve_tt* createDnsResolve(service_tt* pService, bool bLookHostsFile);

frService_API void dnsResolve_addref(dnsResolve_tt* pHandle);

frService_API void dnsResolve_release(dnsResolve_tt* pHandle);

frService_API bool dnsResolve_query(dnsResolve_tt* pHandle, const char* szHostName, bool bIPv6,
                                    uint32_t uiTimeoutMs, uint32_t uiToken);

frService_API int32_t dnsResolve_parser(dnsResolve_tt* pHandle, const char* pBuffer,
                                        size_t nLength);

frService_API const char* dnsResolve_getAddress(dnsResolve_tt* pHandle, int32_t iIndex);

frService_API void dnsResolve_close(dnsResolve_tt* pHandle);

frService_API service_tt* dnsResolve_getService(dnsResolve_tt* pHandle);

// listenPort
frService_API listenPort_tt* createListenPort(service_tt* pService);

frService_API void listenPort_addref(listenPort_tt* pHandle);

frService_API void listenPort_release(listenPort_tt* pHandle);

frService_API bool listenPort_start(listenPort_tt* pHandle, const char* szAddress, bool bTcp);

frService_API void listenPort_close(listenPort_tt* pHandle);

frService_API bool listenPort_postAccept(listenPort_tt* pHandle);

frService_API service_tt* listenPort_getService(listenPort_tt* pHandle);

// timerWatcher
frService_API timerWatcher_tt* createTimerWatcher(service_tt* pService, uint32_t uiToken);

frService_API void timerWatcher_addref(timerWatcher_tt* pHandle);

frService_API void timerWatcher_release(timerWatcher_tt* pHandle);

frService_API bool timerWatcher_start(timerWatcher_tt* pHandle, bool bOnce, uint32_t uiIntervalMs);

frService_API void timerWatcher_stop(timerWatcher_tt* pHandle);

frService_API bool timerWatcher_isRunning(timerWatcher_tt* pHandle);

frService_API service_tt* timerWatcher_getService(timerWatcher_tt* pHandle);
