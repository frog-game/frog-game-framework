

#pragma once

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>

#include "utility_t.h"
#include "thread_t.h"
#include "inetAddress_t.h"
#include "byteQueue_t.h"

typedef enum
{
    eDisconnected,
    eConnecting,
    eConnected,
    eDisconnecting
} enEventConnectionStatus;

typedef enum
{
    eConnectOp,
    eDisconnectOp,
    eRecvOp,
    eSendOp,
} enOperation;

typedef struct eventConnection_overlappedPlus_s
{
    OVERLAPPED                _Overlapped;
    enOperation               eOperation;
    struct eventConnection_s* pEventConnection;
} eventConnection_overlappedPlus_tt;

struct eventBuf_s
{
    void (*fnCallback)(struct eventConnection_s*, void*, bool, uintptr_t);
    eventConnection_overlappedPlus_tt overlapped;
    uint32_t                          uiLength;
    uintptr_t                         uiWriteUser;
    char                              szStorage[];
};

typedef void (*disconnectCallbackPtr)(struct eventConnection_s*, void*);

struct eventListenPort_s;

struct eventConnection_s
{
    bool (*fnReceiveCallback)(struct eventConnection_s*, byteQueue_tt*, void*);
    void (*fnConnectorCallback)(struct eventConnection_s*, void*);
    void (*fnCloseCallback)(struct eventConnection_s*, void*);
    void (*fnUserFree)(void*);
    _Atomic(disconnectCallbackPtr)    hDisconnectCallback;
    void*                             pUserData;
    struct eventIO_s*                 pEventIO;
    eventConnection_overlappedPlus_tt overlapped;
    eventConnection_overlappedPlus_tt disconnectOverlapped;
    SOCKET                            hSocket;
    inetAddress_tt                    remoteAddr;
    inetAddress_tt                    localAddr;
    byteQueue_tt                      readByteQueue;
    struct eventListenPort_s*         pListenPort;
    bool                              bTcp;
    bool                              bReuseSocket;
    atomic_int                        iWritePending;
    atomic_size_t                     nWritePendingBytes;
    atomic_int                        iStatus;
    atomic_int                        iRefCount;
};

__UNUSED struct eventConnection_s* acceptEventConnection(struct eventIO_s*         pEventIO,
                                                         struct eventListenPort_s* pListenPort,
                                                         SOCKET                    hSocket,
                                                         const inetAddress_tt*     pRemoteAddr,
                                                         const inetAddress_tt*     pLocalAddr);

__UNUSED struct eventConnection_s* acceptUdpEventConnection(struct eventIO_s*         pEventIO,
                                                            struct eventListenPort_s* pListenPort,
                                                            SOCKET                    hSocket,
                                                            const inetAddress_tt*     pRemoteAddr,
                                                            const inetAddress_tt*     pLocalAddr);

__UNUSED void eventConnection_onConnect(struct eventConnection_s*          pHandle,
                                        eventConnection_overlappedPlus_tt* pOverlapped);

__UNUSED void eventConnection_onDisconnect(struct eventConnection_s*          pHandle,
                                           eventConnection_overlappedPlus_tt* pOverlapped);

__UNUSED void eventConnection_onRecv(struct eventConnection_s*          pHandle,
                                     eventConnection_overlappedPlus_tt* pOverlapped,
                                     uint32_t                           uiTransferred);

__UNUSED void eventConnection_onSend(struct eventConnection_s*          pHandle,
                                     eventConnection_overlappedPlus_tt* pOverlapped,
                                     uint32_t                           uiTransferred);

__UNUSED void eventConnection_receive(struct eventConnection_s* pHandle, const char* pBuffer,
                                      uint32_t uiLength);