#pragma once

#include <stdatomic.h>

#include "eventIO/internal/posix/poller_t.h"
#include "eventIO/internal/posix/eventIOLoop_t.h"
#include "inetAddress_t.h"
#include "byteQueue_t.h"

typedef enum
{
    eDisconnected,
    eConnecting,
    eConnected,
    eDisconnecting
} enEventConnectionStatus;

struct eventConnection_s;

struct eventBuf_s
{
    void (*fnCallback)(struct eventConnection_s*, void*, bool, uintptr_t);
    eventAsync_tt             eventAsync;
    struct eventConnection_s* pEventConnection;
    uint32_t                  uiLength;
    uintptr_t                 uiWriteUser;
    char                      szStorage[];
};

typedef void (*disconnectCallbackPtr)(struct eventConnection_s*, void*);

struct eventConnection_s
{
    bool (*fnReceiveCallback)(struct eventConnection_s*, byteQueue_tt*, void*);
    void (*fnConnectorCallback)(struct eventConnection_s*, void*);
    void (*fnCloseCallback)(struct eventConnection_s*, void*);
    void (*fnUserFree)(void*);
    _Atomic(disconnectCallbackPtr) hDisconnectCallback;
    void*                          pUserData;
    pollHandle_tt                  pollHandle;
    eventIOLoop_tt*                pEventIOLoop;
    int32_t                        hSocket;
    inetAddress_tt                 remoteAddr;
    inetAddress_tt                 localAddr;
    struct eventListenPort_s*      pListenPort;
    bool                           bTcp;
    bool                           bKeepAlive;
    bool                           bTcpNoDelay;
    byteQueue_tt                   readByteQueue;
    QUEUE                          queueWritePending;
    size_t                         nWritten;
    int32_t                        iWritePending;
    size_t                         nWritePendingBytes;
    atomic_int                     iStatus;
    atomic_int                     iRefCount;
};

__UNUSED struct eventConnection_s* acceptEventConnection(struct eventIO_s*         pEventIO,
                                                         struct eventListenPort_s* pListenPort,
                                                         int32_t                   hSocket,
                                                         const inetAddress_tt*     pRemoteAddr,
                                                         const inetAddress_tt*     pLocalAddr);

__UNUSED struct eventConnection_s* acceptUdpEventConnection(struct eventIO_s*         pEventIO,
                                                            struct eventListenPort_s* pListenPort,
                                                            int32_t                   hSocket,
                                                            const inetAddress_tt*     pRemoteAddr,
                                                            const inetAddress_tt*     pLocalAddr);
