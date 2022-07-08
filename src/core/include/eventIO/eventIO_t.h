#pragma once

#include "byteQueue_t.h"
#include "eventAsync_t.h"
#include "inetAddress_t.h"
#include "platform_t.h"
#include "utility_t.h"


#if DEF_PLATFORM != DEF_PLATFORM_WINDOWS
typedef int32_t SOCKET;
#endif

#ifndef MAXIMUM_MTU_SIZE
#    define MAXIMUM_MTU_SIZE 1492
#endif

struct eventBuf_s;
struct eventTimer_s;
struct eventWatcher_s;
struct eventConnection_s;
struct eventListenPort_s;

typedef struct eventBuf_s        eventBuf_tt;
typedef struct eventTimer_s      eventTimer_tt;
typedef struct eventDgram_s      eventDgram_tt;
typedef struct eventWatcher_s    eventWatcher_tt;
typedef struct eventConnection_s eventConnection_tt;
typedef struct eventListenPort_s eventListenPort_tt;

// eventIO
struct eventIO_s;

typedef struct eventIO_s eventIO_tt;

frCore_API eventIO_tt* createEventIO();

frCore_API void eventIO_addref(eventIO_tt* pEventIO);

frCore_API void eventIO_release(eventIO_tt* pEventIO);

frCore_API void eventIO_setConcurrentThreads(eventIO_tt* pEventIO, uint32_t uiCocurrentThreads);

frCore_API bool eventIO_start(eventIO_tt* pEventIO, bool bTimerEventOff);

frCore_API void eventIO_stop(eventIO_tt* pEventIO);

frCore_API void eventIO_dispatch(eventIO_tt* pEventIO);

frCore_API bool eventIO_isInLoopThread(eventIO_tt* pEventIO);

frCore_API void eventIO_runInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                                  void (*fnWork)(eventAsync_tt*), void (*fnCancel)(eventAsync_tt*));

frCore_API void eventIO_queueInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                                    void (*fnWork)(eventAsync_tt*),
                                    void (*fnCancel)(eventAsync_tt*));

frCore_API uint32_t eventIO_getNumberOfConcurrentThreads(eventIO_tt* pEventIO);

frCore_API int32_t eventIO_getIdleThreads(eventIO_tt* pEventIO);

// eventTimer
frCore_API eventTimer_tt* createEventTimer(eventIO_tt* pEventIO, void (*fn)(eventTimer_tt*, void*),
                                           bool bOnce, uint32_t uiIntervalMs, void* pUserData);

frCore_API void eventTimer_setCloseCallback(eventTimer_tt* pHandle,
                                            void (*fn)(eventTimer_tt*, void*));

frCore_API void eventTimer_addref(eventTimer_tt* pHandle);

frCore_API void eventTimer_release(eventTimer_tt* pHandle);

frCore_API bool eventTimer_start(eventTimer_tt* pHandle);

frCore_API void eventTimer_stop(eventTimer_tt* pHandle);

frCore_API bool eventTimer_isOnce(eventTimer_tt* pHandle);

frCore_API bool eventTimer_isRunning(eventTimer_tt* pHandle);

// eventWatcher
frCore_API eventWatcher_tt* createEventWatcher(eventIO_tt* pEventIO, bool bManualReset,
                                               void (*fn)(eventWatcher_tt*, void*), void* pUserData,
                                               void (*fnUserFree)(void*));

frCore_API void eventWatcher_addref(eventWatcher_tt* pHandle);

frCore_API void eventWatcher_release(eventWatcher_tt* pHandle);

frCore_API bool eventWatcher_start(eventWatcher_tt* pHandle);

frCore_API void eventWatcher_close(eventWatcher_tt* pHandle);

frCore_API bool eventWatcher_isRunning(eventWatcher_tt* pHandle);

frCore_API bool eventWatcher_notify(eventWatcher_tt* pHandle);

frCore_API void eventWatcher_reset(eventWatcher_tt* pHandle);

// eventConnection
frCore_API eventConnection_tt* createEventConnection(eventIO_tt*           pEventIO,
                                                     const inetAddress_tt* pInetAddress, bool bTcp);

frCore_API void eventConnection_setConnectorCallback(eventConnection_tt* pHandle,
                                                     void (*fn)(eventConnection_tt*, void*));

frCore_API void eventConnection_setReceiveCallback(eventConnection_tt* pHandle,
                                                   bool (*fn)(eventConnection_tt*, byteQueue_tt*,
                                                              void*));

frCore_API void eventConnection_setDisconnectCallback(eventConnection_tt* pHandle,
                                                      void (*fn)(eventConnection_tt*, void*));

frCore_API void eventConnection_setCloseCallback(eventConnection_tt* pHandle,
                                                 void (*fn)(eventConnection_tt*, void*));

frCore_API void eventConnection_addref(eventConnection_tt* pHandle);

frCore_API void eventConnection_release(eventConnection_tt* pHandle);

frCore_API bool eventConnection_bind(eventConnection_tt* pHandle, bool bKeepAlive, bool bTcpNoDelay,
                                     void* pUserData, void (*fnUserFree)(void*));

frCore_API bool eventConnection_connect(eventConnection_tt* pHandle, void* pUserData,
                                        void (*fnUserFree)(void*));

frCore_API void eventConnection_close(eventConnection_tt* pHandle);

frCore_API void eventConnection_forceClose(eventConnection_tt* pHandle);

frCore_API void eventConnection_getRemoteAddr(eventConnection_tt* pHandle,
                                              inetAddress_tt*     pOutInetAddress);

frCore_API void eventConnection_getLocalAddr(eventConnection_tt* pHandle,
                                             inetAddress_tt*     pOutInetAddress);

frCore_API bool eventConnection_isConnected(eventConnection_tt* pHandle);

frCore_API bool eventConnection_isConnecting(eventConnection_tt* pHandle);

frCore_API bool eventConnection_isTcp(eventConnection_tt* pHandle);

frCore_API int32_t eventConnection_send(eventConnection_tt* pHandle, eventBuf_tt* pEventBuf);

frCore_API int32_t eventConnection_getWritePending(eventConnection_tt* pHandle);

frCore_API size_t eventConnection_getWritePendingBytes(eventConnection_tt* pHandle);

frCore_API size_t eventConnection_getReceiveBufLength(eventConnection_tt* pHandle);

// eventListenPort
frCore_API eventListenPort_tt* createEventListenPort(eventIO_tt*           pEventIO,
                                                     const inetAddress_tt* pInetAddress, bool bTcp);

frCore_API void eventListenPort_setAcceptCallback(eventListenPort_tt* pHandle,
                                                  void (*fn)(eventListenPort_tt*,
                                                             eventConnection_tt*, const char*,
                                                             uint32_t, void*));

frCore_API void eventListenPort_addref(eventListenPort_tt* pHandle);

frCore_API void eventListenPort_release(eventListenPort_tt* pHandle);

frCore_API bool eventListenPort_start(eventListenPort_tt* pHandle, void* pUserData,
                                      void (*fnUserFree)(void*));

frCore_API void eventListenPort_close(eventListenPort_tt* pHandle);

frCore_API bool eventListenPort_postAccept(eventListenPort_tt* pHandle);

// eventBuf
frCore_API eventBuf_tt* createEventBuf(const char* pBuffer, int32_t iLength,
                                       void (*fn)(eventConnection_tt*, void*, bool, uintptr_t),
                                       uintptr_t uiWriteUser);

frCore_API eventBuf_tt* createEventBuf_move(ioBufVec_tt* pBufVec, int32_t iCount,
                                            void (*fn)(eventConnection_tt*, void*, bool, uintptr_t),
                                            uintptr_t uiWriteUser);

frCore_API void eventBuf_release(eventBuf_tt* pHandle);

// accept recvfrom
frCore_API void setAcceptRecvFromFilterCallback(bool (*fn)(const inetAddress_tt*, const char*,
                                                           uint32_t));
