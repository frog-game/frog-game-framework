

#include "eventIO/internal/win/eventConnection_t.h"

#include <stdlib.h>
#include <assert.h>

#include "utility_t.h"
#include "log_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/internal/win/iocpExt_t.h"
#include "eventIO/internal/win/eventIO-inl.h"
#include "eventIO/internal/win/eventListenPort_t.h"

eventBuf_tt* createEventBuf(const char* pBuffer, int32_t iLength,
                            void (*fn)(eventConnection_tt*, void*, bool, uintptr_t),
                            uintptr_t uiWriteUser)
{
    assert(iLength > 0);
    eventBuf_tt* pEventBuf           = mem_malloc(sizeof(eventBuf_tt) + iLength);
    pEventBuf->overlapped.eOperation = eSendOp;
    bzero(&(pEventBuf->overlapped._Overlapped), sizeof(OVERLAPPED));
    pEventBuf->fnCallback  = fn;
    pEventBuf->uiWriteUser = uiWriteUser;
    pEventBuf->uiLength    = iLength;
    memcpy(pEventBuf->szStorage, pBuffer, iLength);
    return pEventBuf;
}

eventBuf_tt* createEventBuf_move(ioBufVec_tt* pBufVec, int32_t iCount,
                                 void (*fn)(eventConnection_tt*, void*, bool, uintptr_t),
                                 uintptr_t uiWriteUser)
{
    assert(iCount > 0);
    eventBuf_tt* pEventBuf = mem_malloc(sizeof(eventBuf_tt) + sizeof(ioBufVec_tt) * iCount);
    pEventBuf->overlapped.eOperation = eSendOp;
    bzero(&(pEventBuf->overlapped._Overlapped), sizeof(OVERLAPPED));
    pEventBuf->fnCallback  = fn;
    pEventBuf->uiWriteUser = uiWriteUser;
    pEventBuf->uiLength    = (uint32_t)iCount | 0x80000000;
    ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
    for (int32_t i = 0; i < iCount; ++i) {
        pBufWrite[i].pBuf    = pBufVec[i].pBuf;
        pBufWrite[i].iLength = pBufVec[i].iLength;
        pBufVec[i].pBuf      = NULL;
        pBufVec[i].iLength   = 0;
    }
    return pEventBuf;
}

void eventBuf_release(eventBuf_tt* pHandle)
{
    if (pHandle->uiLength & 0x80000000) {
        int32_t      iCount    = pHandle->uiLength & 0x7fffffff;
        ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pHandle->szStorage;
        for (int32_t i = 0; i < iCount; ++i) {
            mem_free(pBufWrite[i].pBuf);
        }
    }
    mem_free(pHandle);
}

static inline void setKeepAlive(int32_t hSocket, bool bOnOff)
{
    int32_t iOptval = bOnOff ? 1 : 0;
    int32_t iResult = setsockopt(
        hSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (iResult != 0) {
        int32_t iError = WSAGetLastError();
        Log(eLog_error, "setsockopt(SO_KEEPALIVE) error, errno=%d", iError);
    }
}

static inline void setTcpNoDelay(int32_t hSocket, bool bOnOff)
{
    int32_t iOptval = bOnOff ? 1 : 0;
    int32_t iResult = setsockopt(
        hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (iResult != 0) {
        int32_t iError = WSAGetLastError();
        Log(eLog_error, "setsockopt(TCP_NODELAY) error, errno=%d", iError);
    }
}

static inline void setDisableUdpConnReset(SOCKET hSocket)
{
    BOOL    bOptval = FALSE;
    DWORD   dwBytes = 0;
    int32_t rc      = WSAIoctl(
        hSocket, SIO_UDP_CONNRESET, &bOptval, sizeof(bOptval), NULL, 0, &dwBytes, NULL, NULL);
    if (rc == SOCKET_ERROR) {
        int32_t iError = WSAGetLastError();
        Log(eLog_error, "WSAIoctl(SIO_UDP_CONNRESET), errno=%d", iError);
    }
}

static inline size_t Max(const size_t a, const size_t b)
{
    return a >= b ? a : b;
}

static void tls_recvBuffer_cleanup_func(void* pArg)
{
    mem_free(pArg);
}

static const size_t g_nRecvBufferMaxLength = 65536;

static inline bool eventConnection_handleRecv(eventConnection_tt* pHandle)
{
    bzero(&(pHandle->overlapped._Overlapped), sizeof(OVERLAPPED));

    size_t nBytesWritable = byteQueue_getBytesWritable(&pHandle->readByteQueue);
    if (nBytesWritable > 0) {
        u_long  uiFlag         = 0;
        int32_t _BufferIOCount = 0;
        WSABUF  _BufferIO[2];

        size_t nContiguousLength;
        char*  pContiguousBytesPointer =
            byteQueue_peekContiguousBytesWrite(&pHandle->readByteQueue, &nContiguousLength);
        if (nContiguousLength == nBytesWritable) {
            _BufferIO[0].buf = pContiguousBytesPointer;
            _BufferIO[0].len = nContiguousLength;
            _BufferIOCount   = 1;
        }
        else {
            _BufferIO[0].buf = pContiguousBytesPointer;
            _BufferIO[0].len = nContiguousLength;
            _BufferIO[1].buf = byteQueue_getBuffer(&pHandle->readByteQueue);
            _BufferIO[1].len = nBytesWritable - nContiguousLength;
            _BufferIOCount   = 2;
        }

        int32_t wsaResult = WSARecv(pHandle->hSocket,
                                    _BufferIO,
                                    _BufferIOCount,
                                    NULL,
                                    &uiFlag,
                                    &(pHandle->overlapped._Overlapped),
                                    NULL);
        if (wsaResult == SOCKET_ERROR) {
            int32_t iError = WSAGetLastError();
            if (iError != WSA_IO_PENDING) {
                return false;
            }
        }
    }
    else {
        u_long uiFlag = MSG_PEEK;
        WSABUF _BufferIO;
        _BufferIO.buf = NULL;
        _BufferIO.len = 0;

        int32_t wsaResult = WSARecv(pHandle->hSocket,
                                    &_BufferIO,
                                    1,
                                    NULL,
                                    &uiFlag,
                                    &(pHandle->overlapped._Overlapped),
                                    NULL);
        if (wsaResult == SOCKET_ERROR) {
            int32_t iError = WSAGetLastError();
            if (iError != WSA_IO_PENDING) {
                return false;
            }
        }
    }
    return true;
}

void eventConnection_onConnect(struct eventConnection_s*          pHandle,
                               eventConnection_overlappedPlus_tt* pOverlapped)
{
    if (atomic_load(&pHandle->iStatus) == eConnecting) {
        if (NT_SUCCESS(pOverlapped->_Overlapped.Internal)) {
            if (pHandle->bTcp) {
                setsockopt(pHandle->hSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
            }

            socklen_t addrlen = sizeof pHandle->localAddr;
            if (getsockname(
                    pHandle->hSocket, inetAddress_getSockaddr(&pHandle->localAddr), &addrlen) < 0) {
                int32_t iStatus = eConnecting;
                if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eDisconnected)) {
                    closesocket(pHandle->hSocket);
                    pHandle->hSocket = INVALID_SOCKET;
                }
            }

            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
        }
        else {
            int32_t iStatus = eConnecting;
            if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eDisconnected)) {
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
            }
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
        }
    }
    eventConnection_release(pHandle);
}

void eventConnection_onDisconnect(struct eventConnection_s*          pHandle,
                                  eventConnection_overlappedPlus_tt* pOverlapped)
{
    int32_t iStatus = eDisconnecting;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eDisconnected)) {
        if (pHandle->hSocket != INVALID_SOCKET) {
            if (pHandle->bReuseSocket) {
                eventIO_recoverySocket(pHandle->pEventIO, pHandle->hSocket);
            }
            else {
                closesocket(pHandle->hSocket);
            }
            pHandle->hSocket = INVALID_SOCKET;
        }

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
            pHandle->fnCloseCallback = NULL;
        }
    }
    eventConnection_release(pHandle);
}

void eventConnection_receive(struct eventConnection_s* pHandle, const char* pBuffer,
                             uint32_t uiLength)
{
    if (atomic_load(&pHandle->iStatus) == eConnected) {
        byteQueue_writeBytes(&pHandle->readByteQueue, pBuffer, uiLength);

        if (pHandle->fnReceiveCallback) {
            size_t nBytesReadable = byteQueue_getBytesReadable(&pHandle->readByteQueue);

            if (!pHandle->fnReceiveCallback(pHandle, &pHandle->readByteQueue, pHandle->pUserData)) {
                disconnectCallbackPtr fnDisconnectCallback =
                    (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, NULL);
                if (fnDisconnectCallback) {
                    fnDisconnectCallback(pHandle, pHandle->pUserData);
                }
            }

            if (nBytesReadable * 2 < byteQueue_getCapacity(&pHandle->readByteQueue)) {
                if (pHandle->bTcp) {
                    byteQueue_reserve(&pHandle->readByteQueue,
                                      Max(256, byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                }
                else {
                    byteQueue_reserve(
                        &pHandle->readByteQueue,
                        Max(MAXIMUM_MTU_SIZE, byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                }
            }
        }
    }
}

void eventConnection_onRecv(struct eventConnection_s*          pHandle,
                            eventConnection_overlappedPlus_tt* pOverlapped, uint32_t uiTransferred)
{
    if (atomic_load(&pHandle->iStatus) != eConnected) {
        eventConnection_release(pHandle);
    }
    else {
        if (uiTransferred != 0) {
            byteQueue_writeOffset(&pHandle->readByteQueue, uiTransferred);
        }

        if (uiTransferred == 0 || byteQueue_getBytesWritable(&pHandle->readByteQueue) == 0) {
            static _decl_threadLocal char* s_pRecvBuffer = NULL;
            if (s_pRecvBuffer == NULL) {
                s_pRecvBuffer = (char*)mem_malloc(g_nRecvBufferMaxLength);
                setTlsValue(pHandle, tls_recvBuffer_cleanup_func, s_pRecvBuffer, true);
            }
            WSABUF _BufferIO;
            _BufferIO.buf = s_pRecvBuffer;
            _BufferIO.len = g_nRecvBufferMaxLength;

            u_long  uiFlag               = 0;
            DWORD   dwNumberOfBytesRecvd = 0;
            int32_t wsaResult            = WSARecv(
                pHandle->hSocket, &_BufferIO, 1, &dwNumberOfBytesRecvd, &uiFlag, NULL, NULL);
            if (wsaResult == SOCKET_ERROR) {
                int32_t iError = WSAGetLastError();
                if (iError != WSAEWOULDBLOCK) {
                    disconnectCallbackPtr fnDisconnectCallback =
                        (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, 0);
                    if (fnDisconnectCallback) {
                        fnDisconnectCallback(pHandle, pHandle->pUserData);
                    }
                    eventConnection_release(pHandle);
                    return;
                }
            }

            if (dwNumberOfBytesRecvd > 0) {
                byteQueue_writeBytes(&pHandle->readByteQueue, s_pRecvBuffer, dwNumberOfBytesRecvd);
            }
            else {
                if (uiTransferred == 0) {
                    disconnectCallbackPtr fnDisconnectCallback =
                        (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, 0);
                    if (fnDisconnectCallback) {
                        fnDisconnectCallback(pHandle, pHandle->pUserData);
                    }
                    eventConnection_release(pHandle);
                    return;
                }
            }
        }

        if (pHandle->fnReceiveCallback) {
            size_t nBytesReadable = byteQueue_getBytesReadable(&pHandle->readByteQueue);

            if (pHandle->fnReceiveCallback(pHandle, &pHandle->readByteQueue, pHandle->pUserData)) {
                if (nBytesReadable * 2 < byteQueue_getCapacity(&pHandle->readByteQueue)) {
                    if (pHandle->bTcp) {
                        byteQueue_reserve(
                            &pHandle->readByteQueue,
                            Max(256, byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                    }
                    else {
                        byteQueue_reserve(&pHandle->readByteQueue,
                                          Max(MAXIMUM_MTU_SIZE,
                                              byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                    }
                }

                if (!eventConnection_handleRecv(pHandle)) {
                    disconnectCallbackPtr fnDisconnectCallback =
                        (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, NULL);
                    if (fnDisconnectCallback) {
                        fnDisconnectCallback(pHandle, pHandle->pUserData);
                    }
                    eventConnection_release(pHandle);
                    return;
                }
            }
            else {
                disconnectCallbackPtr fnDisconnectCallback =
                    (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, NULL);
                if (fnDisconnectCallback) {
                    fnDisconnectCallback(pHandle, pHandle->pUserData);
                }
                eventConnection_release(pHandle);
            }
        }
    }
}

void eventConnection_onSend(struct eventConnection_s*          pHandle,
                            eventConnection_overlappedPlus_tt* pOverlapped, uint32_t uiTransferred)
{
    eventBuf_tt* pEventBuf = container_of(pOverlapped, eventBuf_tt, overlapped);

    uint32_t uiLength = 0;

    if (pEventBuf->uiLength & 0x80000000) {
        int32_t      iCount    = pEventBuf->uiLength & 0x7fffffff;
        ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
        for (int32_t i = 0; i < iCount; ++i) {
            uiLength += pBufWrite[i].iLength;
        }
    }
    else {
        uiLength = pEventBuf->uiLength & 0x7fffffff;
    }

    atomic_fetch_sub(&(pHandle->nWritePendingBytes), uiLength);
    atomic_fetch_sub(&(pHandle->iWritePending), 1);

    if (atomic_load(&pHandle->iStatus) != eConnected || uiTransferred == 0 ||
        uiTransferred != uiLength) {
        if (pEventBuf->fnCallback) {
            pEventBuf->fnCallback(pHandle, pHandle->pUserData, false, pEventBuf->uiWriteUser);
        }

        if (atomic_load(&pHandle->iWritePending) == 0) {
            disconnectCallbackPtr fnDisconnectCallback =
                (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, 0);
            if (fnDisconnectCallback) {
                fnDisconnectCallback(pHandle, pHandle->pUserData);
            }
        }
    }
    else {
        if (pEventBuf->fnCallback) {
            pEventBuf->fnCallback(pHandle, pHandle->pUserData, true, pEventBuf->uiWriteUser);
        }
    }

    eventBuf_release(pEventBuf);
    eventConnection_release(pHandle);
}

struct eventConnection_s* acceptUdpEventConnection(struct eventIO_s*         pEventIO,
                                                   struct eventListenPort_s* pListenPort,
                                                   SOCKET                    hSocket,
                                                   const inetAddress_tt*     pRemoteAddr,
                                                   const inetAddress_tt*     pLocalAddr)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIO            = pEventIO;
    pHandle->fnUserFree          = NULL;
    pHandle->pUserData           = NULL;
    pHandle->fnConnectorCallback = NULL;
    pHandle->fnReceiveCallback   = NULL;
    pHandle->fnCloseCallback     = NULL;
    atomic_init(&pHandle->hDisconnectCallback, 0);
    pHandle->hSocket     = hSocket;
    pHandle->remoteAddr  = *pRemoteAddr;
    pHandle->localAddr   = *pLocalAddr;
    pHandle->pListenPort = pListenPort;
    eventListenPort_addref(pHandle->pListenPort);
    pHandle->bReuseSocket = false;
    pHandle->bTcp         = false;

    pHandle->overlapped.pEventConnection = pHandle;
    pHandle->overlapped.eOperation       = eConnectOp;

    pHandle->disconnectOverlapped.pEventConnection = pHandle;
    pHandle->disconnectOverlapped.eOperation       = eDisconnectOp;

    byteQueue_init(&pHandle->readByteQueue, 0);
    atomic_init(&pHandle->iWritePending, 0);
    atomic_init(&pHandle->nWritePendingBytes, 0);
    atomic_init(&pHandle->iStatus, eConnecting);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

struct eventConnection_s* acceptEventConnection(struct eventIO_s*         pEventIO,
                                                struct eventListenPort_s* pListenPort,
                                                SOCKET hSocket, const inetAddress_tt* pRemoteAddr,
                                                const inetAddress_tt* pLocalAddr)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIO            = pEventIO;
    pHandle->fnUserFree          = NULL;
    pHandle->pUserData           = NULL;
    pHandle->fnConnectorCallback = NULL;
    pHandle->fnReceiveCallback   = NULL;
    pHandle->fnCloseCallback     = NULL;
    atomic_init(&pHandle->hDisconnectCallback, 0);
    pHandle->hSocket     = hSocket;
    pHandle->remoteAddr  = *pRemoteAddr;
    pHandle->localAddr   = *pLocalAddr;
    pHandle->pListenPort = pListenPort;
    eventListenPort_addref(pHandle->pListenPort);
    pHandle->bReuseSocket = true;
    pHandle->bTcp         = true;

    pHandle->overlapped.pEventConnection = pHandle;
    pHandle->overlapped.eOperation       = eConnectOp;

    pHandle->disconnectOverlapped.pEventConnection = pHandle;
    pHandle->disconnectOverlapped.eOperation       = eDisconnectOp;

    byteQueue_init(&pHandle->readByteQueue, 256);
    atomic_init(&pHandle->iWritePending, 0);
    atomic_init(&pHandle->nWritePendingBytes, 0);
    atomic_init(&pHandle->iStatus, eConnecting);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

eventConnection_tt* createEventConnection(eventIO_tt* pEventIO, const inetAddress_tt* pInetAddress,
                                          bool bTcp)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIO            = pEventIO;
    pHandle->pUserData           = NULL;
    pHandle->fnConnectorCallback = NULL;
    pHandle->fnReceiveCallback   = NULL;
    pHandle->fnCloseCallback     = NULL;
    atomic_init(&pHandle->hDisconnectCallback, 0);
    pHandle->fnUserFree   = NULL;
    pHandle->hSocket      = INVALID_SOCKET;
    pHandle->remoteAddr   = *pInetAddress;
    pHandle->pListenPort  = NULL;
    pHandle->bReuseSocket = false;
    pHandle->bTcp         = bTcp;

    pHandle->overlapped.pEventConnection = pHandle;
    pHandle->overlapped.eOperation       = eConnectOp;

    pHandle->disconnectOverlapped.pEventConnection = pHandle;
    pHandle->disconnectOverlapped.eOperation       = eDisconnectOp;

    if (bTcp) {
        byteQueue_init(&pHandle->readByteQueue, 256);
    }
    else {
        byteQueue_init(&pHandle->readByteQueue, MAXIMUM_MTU_SIZE);
    }
    atomic_init(&pHandle->iWritePending, 0);
    atomic_init(&pHandle->nWritePendingBytes, 0);
    atomic_init(&pHandle->iStatus, eDisconnected);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void eventConnection_setConnectorCallback(eventConnection_tt* pHandle,
                                          void (*fn)(eventConnection_tt*, void*))
{
    pHandle->fnConnectorCallback = fn;
}

void eventConnection_setReceiveCallback(eventConnection_tt* pHandle,
                                        bool (*fn)(eventConnection_tt*, byteQueue_tt*, void*))
{
    pHandle->fnReceiveCallback = fn;
}

void eventConnection_setDisconnectCallback(eventConnection_tt* pHandle,
                                           void (*fn)(eventConnection_tt*, void*))
{
    atomic_store(&pHandle->hDisconnectCallback, fn);
}

void eventConnection_setCloseCallback(eventConnection_tt* pHandle,
                                      void (*fn)(eventConnection_tt*, void*))
{
    pHandle->fnCloseCallback = fn;
}

void eventConnection_addref(eventConnection_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void eventConnection_release(eventConnection_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->pListenPort) {
            if (!pHandle->bTcp) {
                eventListenPort_removeAddressConnection(pHandle->pListenPort, &pHandle->remoteAddr);
            }
            eventListenPort_release(pHandle->pListenPort);
            pHandle->pListenPort = NULL;
        }

        if (pHandle->fnUserFree) {
            pHandle->fnUserFree(pHandle->pUserData);
        }
        mem_free(pHandle);
    }
}

bool eventConnection_bind(eventConnection_tt* pHandle, bool bKeepAlive, bool bTcpNoDelay,
                          void* pUserData, void (*fnUserFree)(void*))
{
    int32_t iStatus = eConnecting;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eConnected)) {
        if (pUserData) {
            if (pHandle->fnUserFree && pHandle->pUserData) {
                pHandle->fnUserFree(pHandle->pUserData);
            }
            pHandle->pUserData  = pUserData;
            pHandle->fnUserFree = fnUserFree;
        }

        if (pHandle->bTcp) {
            setKeepAlive(pHandle->hSocket, bKeepAlive);
            setTcpNoDelay(pHandle->hSocket, bTcpNoDelay);
        }

        if (!(pHandle->pListenPort && !pHandle->bTcp)) {
            pHandle->overlapped.eOperation = eRecvOp;
            atomic_fetch_add(&(pHandle->iRefCount), 1);

            if (eventConnection_handleRecv(pHandle)) {
                return true;
            }

            atomic_store(&pHandle->iStatus, eDisconnected);
            if (pHandle->hSocket != INVALID_SOCKET) {
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
            }
            eventConnection_release(pHandle);
        }
        else {
            return true;
        }
    }
    return false;
}

static int32_t setSocketNonblocking(SOCKET hSocket)
{
    unsigned long nb = 1;
    return ioctlsocket(hSocket, FIONBIO, &nb);
}

bool eventConnection_connect(eventConnection_tt* pHandle, void* pUserData,
                             void (*fnUserFree)(void*))
{
    int32_t iStatus = eDisconnected;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eConnecting)) {
        if (pHandle->fnUserFree && pHandle->pUserData) {
            pHandle->fnUserFree(pHandle->pUserData);
        }
        pHandle->pUserData  = pUserData;
        pHandle->fnUserFree = fnUserFree;

        if (pHandle->hSocket == INVALID_SOCKET) {
            if (pHandle->bTcp) {
                pHandle->hSocket =
                    WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

                if (pHandle->hSocket == INVALID_SOCKET) {
                    atomic_store(&pHandle->iStatus, eDisconnected);
                    return false;
                }
            }
            else {
                pHandle->hSocket =
                    WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
                if (pHandle->hSocket == INVALID_SOCKET) {
                    atomic_store(&pHandle->iStatus, eDisconnected);
                    return false;
                }
                setDisableUdpConnReset(pHandle->hSocket);
            }

            setSocketNonblocking(pHandle->hSocket);

            struct sockaddr_in localAddress;
            memset(&localAddress, 0, sizeof(struct sockaddr_in));
            localAddress.sin_family      = AF_INET;
            localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
            localAddress.sin_port        = htons(0);

            if (bind(pHandle->hSocket,
                     (struct sockaddr*)&localAddress,
                     sizeof(struct sockaddr_in)) < 0) {
                int32_t iError = WSAGetLastError();
                if (iError != WSAEINVAL) {
                    atomic_store(&pHandle->iStatus, eDisconnected);
                    closesocket(pHandle->hSocket);
                    pHandle->hSocket = INVALID_SOCKET;
                    return false;
                }
            }

            if (CreateIoCompletionPort((HANDLE)pHandle->hSocket,
                                       pHandle->pEventIO->hCompletionPort,
                                       def_IOCP_CONNECTION,
                                       0) == NULL) {
                atomic_store(&pHandle->iStatus, eDisconnected);
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
                return false;
            }

            bzero(&(pHandle->overlapped._Overlapped), sizeof(OVERLAPPED));

            atomic_fetch_add(&(pHandle->iRefCount), 1);
            if (pHandle->bTcp) {
                BOOL bConnectResult = connectEx(pHandle->hSocket,
                                                inetAddress_getSockaddr(&pHandle->remoteAddr),
                                                inetAddress_getSocklen(&pHandle->remoteAddr),
                                                NULL,
                                                0,
                                                NULL,
                                                &(pHandle->overlapped._Overlapped));
                if (bConnectResult == FALSE) {
                    int32_t iError = WSAGetLastError();
                    if (iError != ERROR_IO_PENDING) {
                        atomic_store(&pHandle->iStatus, eDisconnected);
                        closesocket(pHandle->hSocket);
                        pHandle->hSocket = INVALID_SOCKET;
                        eventConnection_release(pHandle);
                        return false;
                    }
                }
            }
            else {
                int32_t iConnectResult = WSAConnect(pHandle->hSocket,
                                                    inetAddress_getSockaddr(&pHandle->remoteAddr),
                                                    inetAddress_getSocklen(&pHandle->remoteAddr),
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL);
                if (iConnectResult == SOCKET_ERROR) {
                    int32_t iError = WSAGetLastError();
                    if (iError != WSAEWOULDBLOCK) {
                        atomic_store(&pHandle->iStatus, eDisconnected);
                        closesocket(pHandle->hSocket);
                        pHandle->hSocket = INVALID_SOCKET;
                        return false;
                    }
                }
                PostQueuedCompletionStatus(pHandle->pEventIO->hCompletionPort,
                                           0,
                                           def_IOCP_CONNECTION,
                                           &(pHandle->overlapped._Overlapped));
            }
            return true;
        }
        else {
            return false;
        }
    }
    return false;
}

void eventConnection_forceClose(eventConnection_tt* pHandle)
{
    int32_t iStatus = atomic_exchange(&pHandle->iStatus, eDisconnected);
    if (iStatus != eDisconnected) {
        if (pHandle->hSocket != INVALID_SOCKET) {
            closesocket(pHandle->hSocket);
            pHandle->hSocket = INVALID_SOCKET;
        }

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
            pHandle->fnCloseCallback = NULL;
        }
    }
}

void eventConnection_close(eventConnection_tt* pHandle)
{
    int32_t iStatus = eConnected;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eDisconnecting)) {
        if (pHandle->bTcp) {
            DWORD dwFlags = 0;
            if (pHandle->bReuseSocket) {
                dwFlags = TF_REUSE_SOCKET;
            }

            bzero(&(pHandle->disconnectOverlapped._Overlapped), sizeof(OVERLAPPED));
            atomic_fetch_add(&(pHandle->iRefCount), 1);
            if (!disconnectEx(
                    pHandle->hSocket, &(pHandle->disconnectOverlapped._Overlapped), dwFlags, 0)) {
                int32_t iError = WSAGetLastError();
                if (iError != ERROR_IO_PENDING) {
                    eventConnection_forceClose(pHandle);
                    eventConnection_release(pHandle);
                }
            }
        }
        else {
            eventConnection_forceClose(pHandle);
            eventConnection_release(pHandle);
        }
    }
}

void eventConnection_getRemoteAddr(eventConnection_tt* pHandle, inetAddress_tt* pOutInetAddress)
{
    *pOutInetAddress = pHandle->remoteAddr;
}

void eventConnection_getLocalAddr(eventConnection_tt* pHandle, inetAddress_tt* pOutInetAddress)
{
    *pOutInetAddress = pHandle->localAddr;
}

bool eventConnection_isConnecting(eventConnection_tt* pHandle)
{
    return atomic_load(&pHandle->iStatus) == eConnecting;
}

bool eventConnection_isConnected(eventConnection_tt* pHandle)
{
    return atomic_load(&pHandle->iStatus) == eConnected;
}

bool eventConnection_isTcp(eventConnection_tt* pHandle)
{
    return pHandle->bTcp;
}

int32_t eventConnection_send(eventConnection_tt* pHandle, eventBuf_tt* pEventBuf)
{
    if (atomic_load(&pHandle->iStatus) != eConnected) {
        eventBuf_release(pEventBuf);
        return -1;
    }

    pEventBuf->overlapped.pEventConnection = pHandle;
    eventConnection_addref(pEventBuf->overlapped.pEventConnection);
    atomic_fetch_add(&(pHandle->iWritePending), 1);

    int32_t iLength = 0;
    if (pEventBuf->uiLength & 0x80000000) {
        ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;

        const int32_t iCount = pEventBuf->uiLength & 0x7fffffff;
        WSABUF        _BufferIO[iCount];
        for (int32_t i = 0; i < iCount; ++i) {
            _BufferIO[i].buf = pBufWrite[i].pBuf;
            _BufferIO[i].len = pBufWrite[i].iLength;
            iLength += pBufWrite[i].iLength;
        }

        atomic_fetch_add(&pHandle->nWritePendingBytes, iLength);
        int32_t wsaResult = WSASend(pHandle->hSocket,
                                    _BufferIO,
                                    iCount,
                                    NULL,
                                    0,
                                    &(pEventBuf->overlapped._Overlapped),
                                    NULL);
        if (wsaResult == SOCKET_ERROR) {
            int32_t iError = WSAGetLastError();
            if (iError != WSA_IO_PENDING) {
                atomic_fetch_sub(&(pHandle->iWritePending), 1);
                atomic_fetch_sub(&(pHandle->nWritePendingBytes), iLength);
                eventBuf_release(pEventBuf);
                eventConnection_release(pHandle);
                return -1;
            }
        }
    }
    else {
        iLength = pEventBuf->uiLength & 0x7fffffff;
        atomic_fetch_add(&(pHandle->nWritePendingBytes), iLength);

        WSABUF _BufferIO;
        _BufferIO.buf = pEventBuf->szStorage;
        _BufferIO.len = iLength;

        int32_t wsaResult = WSASend(
            pHandle->hSocket, &_BufferIO, 1, NULL, 0, &(pEventBuf->overlapped._Overlapped), NULL);
        if (wsaResult == SOCKET_ERROR) {
            int32_t iError = WSAGetLastError();
            if (iError != WSA_IO_PENDING) {
                atomic_fetch_sub(&(pHandle->iWritePending), 1);
                atomic_fetch_sub(&(pHandle->nWritePendingBytes), iLength);
                eventBuf_release(pEventBuf);
                eventConnection_release(pHandle);
                return -1;
            }
        }
    }
    return iLength;
}

int32_t eventConnection_getWritePending(eventConnection_tt* pHandle)
{
    return atomic_load(&pHandle->iWritePending);
}

size_t eventConnection_getWritePendingBytes(eventConnection_tt* pHandle)
{
    return atomic_load(&pHandle->nWritePendingBytes);
}

size_t eventConnection_getReceiveBufLength(eventConnection_tt* pHandle)
{
    return byteQueue_getCapacity(&pHandle->readByteQueue);
}
