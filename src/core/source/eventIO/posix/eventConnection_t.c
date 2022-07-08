#include "eventIO/internal/posix/eventConnection_t.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>
#include "utility_t.h"
#include "log_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/internal/posix/eventListenPort_t.h"

eventBuf_tt* createEventBuf(const char* pBuffer, int32_t iLength,
                            void (*fn)(eventConnection_tt*, void*, bool, uintptr_t),
                            uintptr_t uiWriteUser)
{
    assert(iLength > 0);
    eventBuf_tt* pEventBuf = mem_malloc(sizeof(eventBuf_tt) + iLength);
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

#define SHUTDOWN_WR SHUT_WR

#ifndef MSG_NOSIGNAL
#    define MSG_NOSIGNAL 0
#endif

#define TEST_ERR_RW_RETRIABLE(e) ((e) == EINTR || (e) == EAGAIN)

#define TEST_ERR_CONNECT_RETRIABLE(e) ((e) == EINTR || (e) == EINPROGRESS)

static inline void setKeepAlive(int32_t hSocket, bool bOnOff)
{
    int32_t iOptval = bOnOff ? 1 : 0;
    int32_t iResult = setsockopt(
        hSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (iResult != 0) {
        int32_t iError = errno;
        Log(eLog_error, "setsockopt(SO_KEEPALIVE) error, errno=%d", iError);
    }
}

static inline void setTcpNoDelay(int32_t hSocket, bool bOnOff)
{
    int32_t iOptval = bOnOff ? 1 : 0;
    int32_t iResult = setsockopt(
        hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (iResult != 0) {
        int32_t iError = errno;
        Log(eLog_error, "setsockopt(TCP_NODELAY) error, errno=%d", iError);
    }
}

static inline int32_t Min(const int32_t a, const int32_t b)
{
    return a < b ? a : b;
}

static inline size_t Max(const size_t a, const size_t b)
{
    return a >= b ? a : b;
}

static const size_t g_nRecvBufferMaxLength = 65536;

static const size_t g_nSendBufferMaxLength = 8192;

static inline void eventConnection_insertQueueWritePending(eventConnection_tt* pHandle,
                                                           eventAsync_tt*      pEventAsync)
{
    QUEUE_INSERT_TAIL(&pHandle->queueWritePending, &pEventAsync->node);
}

static inline eventBuf_tt* eventConnection_getEventConnectionWrite(eventConnection_tt* pHandle,
                                                                   QUEUE*              pNode)
{
    eventAsync_tt* pEventAsync = container_of(pNode, eventAsync_tt, node);
    return container_of(pEventAsync, eventBuf_tt, eventAsync);
}

static inline void eventConnection_handleClose(eventConnection_tt* pHandle)
{
    if (pHandle->hSocket != -1) {
        atomic_store(&pHandle->iStatus, eDisconnected);
        poller_clear(
            pHandle->pEventIOLoop->pPoller, pHandle->hSocket, &pHandle->pollHandle, ePollerClosed);
        close(pHandle->hSocket);
        pHandle->hSocket = -1;

        eventBuf_tt* pEventBuf    = NULL;
        QUEUE*       pNode        = NULL;
        size_t       nWriteLength = 0;
        while (!QUEUE_EMPTY(&pHandle->queueWritePending)) {
            pNode     = QUEUE_HEAD(&pHandle->queueWritePending);
            pEventBuf = eventConnection_getEventConnectionWrite(pHandle, pNode);

            if (pEventBuf->uiLength & 0x80000000) {
                int32_t      iCount    = pEventBuf->uiLength & 0x7fffffff;
                ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
                for (int32_t i = 0; i < iCount; ++i) {
                    nWriteLength += pBufWrite[i].iLength;
                }
            }
            else {
                nWriteLength = pEventBuf->uiLength & 0x7fffffff;
            }
            QUEUE_REMOVE(pNode);
            --pHandle->iWritePending;
            pHandle->nWritePendingBytes -= nWriteLength;
            if (pEventBuf->fnCallback) {
                pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                      pEventBuf->pEventConnection->pUserData,
                                      false,
                                      pEventBuf->uiWriteUser);
            }
            eventBuf_release(pEventBuf);
            pHandle->nWritten = 0;
        }

        if (pHandle->fnCloseCallback) {
            pHandle->fnCloseCallback(pHandle, pHandle->pUserData);
            pHandle->fnCloseCallback = NULL;
        }
        atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
        eventConnection_release(pHandle);
    }
}

static void tls_recvBuffer_cleanup_func(void* pArg)
{
    mem_free(pArg);
}

static bool eventConnection_handleRecv(eventConnection_tt* pHandle)
{
    static _decl_threadLocal char* s_pRecvBuffer = NULL;
    if (s_pRecvBuffer == NULL) {
        s_pRecvBuffer = (char*)mem_malloc(g_nRecvBufferMaxLength);
        setTlsValue(pHandle, tls_recvBuffer_cleanup_func, s_pRecvBuffer, true);
    }

    int32_t iBytesRead     = 0;
    size_t  nBytesWritable = byteQueue_getBytesWritable(&pHandle->readByteQueue);
    if (nBytesWritable == 0) {
        iBytesRead = recv(pHandle->hSocket, s_pRecvBuffer, g_nRecvBufferMaxLength, 0);
    }
    else {
        int32_t _BufferIOCount = 0;

        struct iovec _BufferIO[3];
        size_t       nContiguousLength;
        char*        pContiguousBytesPointer =
            byteQueue_peekContiguousBytesWrite(&pHandle->readByteQueue, &nContiguousLength);
        if (nContiguousLength == nBytesWritable) {
            _BufferIO[0].iov_base = pContiguousBytesPointer;
            _BufferIO[0].iov_len  = nContiguousLength;
            _BufferIO[1].iov_base = s_pRecvBuffer;
            _BufferIO[1].iov_len  = g_nRecvBufferMaxLength;
            _BufferIOCount        = (nContiguousLength < g_nRecvBufferMaxLength) ? 2 : 1;
        }
        else {
            _BufferIO[0].iov_base = pContiguousBytesPointer;
            _BufferIO[0].iov_len  = nContiguousLength;
            _BufferIO[1].iov_base = byteQueue_getBuffer(&pHandle->readByteQueue);
            _BufferIO[1].iov_len  = nBytesWritable - nContiguousLength;
            _BufferIO[2].iov_base = s_pRecvBuffer;
            _BufferIO[2].iov_len  = g_nRecvBufferMaxLength;
            _BufferIOCount        = (nBytesWritable < g_nRecvBufferMaxLength) ? 3 : 2;
        }
        iBytesRead = readv(pHandle->hSocket, _BufferIO, _BufferIOCount);
    }

    if (iBytesRead > 0) {
        if ((size_t)(iBytesRead) <= nBytesWritable) {
            byteQueue_writeOffset(&pHandle->readByteQueue, iBytesRead);

            if (pHandle->fnReceiveCallback) {
                size_t nBytesReadable = byteQueue_getBytesReadable(&pHandle->readByteQueue);
                if (pHandle->fnReceiveCallback(
                        pHandle, &pHandle->readByteQueue, pHandle->pUserData)) {
                    if (nBytesReadable * 2 < byteQueue_getCapacity(&pHandle->readByteQueue)) {
                        if (pHandle->bTcp) {
                            byteQueue_reserve(
                                &pHandle->readByteQueue,
                                Max(256, byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                        }
                        else {
                            byteQueue_reserve(
                                &pHandle->readByteQueue,
                                Max(MAXIMUM_MTU_SIZE,
                                    byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                        }
                    }
                    return true;
                }
                return false;
            }
        }
        else {
            if (nBytesWritable != 0) {
                byteQueue_writeOffset(&pHandle->readByteQueue, nBytesWritable);
            }
            byteQueue_writeBytes(
                &pHandle->readByteQueue, s_pRecvBuffer, iBytesRead - nBytesWritable);
            if (pHandle->fnReceiveCallback) {
                size_t nBytesReadable = byteQueue_getBytesReadable(&pHandle->readByteQueue);
                if (pHandle->fnReceiveCallback(
                        pHandle, &pHandle->readByteQueue, pHandle->pUserData)) {
                    if (nBytesReadable * 2 < byteQueue_getCapacity(&pHandle->readByteQueue)) {
                        if (pHandle->bTcp) {
                            byteQueue_reserve(
                                &pHandle->readByteQueue,
                                Max(256, byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                        }
                        else {
                            byteQueue_reserve(
                                &pHandle->readByteQueue,
                                Max(MAXIMUM_MTU_SIZE,
                                    byteQueue_getCapacity(&pHandle->readByteQueue) / 2));
                        }
                    }
                    return true;
                }
                return false;
            }
        }
    }
    else if (iBytesRead < 0) {
        int32_t iError = errno;
        if (TEST_ERR_RW_RETRIABLE(iError)) {
            return true;
        }
    }
    return false;
}

static inline void eventConnection_sendCompleteCallback(eventConnection_tt* pHandle)
{
    eventBuf_tt* pEventBuf = NULL;
    QUEUE*       pNode     = NULL;
    size_t       nLength   = 0;

    while (!QUEUE_EMPTY(&pHandle->queueWritePending)) {
        pNode     = QUEUE_HEAD(&pHandle->queueWritePending);
        pEventBuf = eventConnection_getEventConnectionWrite(pHandle, pNode);
        if (pEventBuf->uiLength & 0x80000000) {
            int32_t      iCount    = pEventBuf->uiLength & 0x7fffffff;
            ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
            for (int32_t i = 0; i < iCount; ++i) {
                nLength += pBufWrite[i].iLength;
            }
        }
        else {
            nLength = pEventBuf->uiLength & 0x7fffffff;
        }

        if (pHandle->nWritten >= nLength) {
            QUEUE_REMOVE(pNode);
            --pHandle->iWritePending;
            pHandle->nWritePendingBytes -= nLength;
            pHandle->nWritten -= nLength;
            if (pEventBuf->fnCallback) {
                pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                      pEventBuf->pEventConnection->pUserData,
                                      true,
                                      pEventBuf->uiWriteUser);
            }
            eventBuf_release(pEventBuf);
        }
        else {
            break;
        }
    }

    if (pHandle->iWritePending > 0) {
        size_t  nWriteLength = 0;
        int32_t iBytesSent   = 0;

        if (pHandle->nWritten != 0) {
            pNode     = QUEUE_HEAD(&pHandle->queueWritePending);
            pEventBuf = eventConnection_getEventConnectionWrite(pHandle, pNode);

            if (pEventBuf->uiLength & 0x80000000) {
                const int32_t iCount    = pEventBuf->uiLength & 0x7fffffff;
                ioBufVec_tt*  pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
                struct iovec  _BufferIO[iCount];
                int32_t       iSendCount  = 0;
                int32_t       iSkipLength = pHandle->nWritten;
                for (int32_t i = 0; i < iCount; ++i) {
                    if (iSkipLength >= pBufWrite[i].iLength) {
                        iSkipLength -= pBufWrite[i].iLength;
                    }
                    else {
                        _BufferIO[iSendCount].iov_base = pBufWrite[i].pBuf + iSkipLength;
                        _BufferIO[iSendCount].iov_len  = pBufWrite[i].iLength - iSkipLength;
                        iSkipLength                    = 0;
                        ++iSendCount;
                    }
                }
                iBytesSent = writev(pHandle->hSocket, _BufferIO, iSendCount);
            }
            else {
                nLength    = pEventBuf->uiLength & 0x7fffffff;
                iBytesSent = send(pHandle->hSocket,
                                  pEventBuf->szStorage + pHandle->nWritten,
                                  nLength - pHandle->nWritten,
                                  MSG_NOSIGNAL);
            }
        }
        else {
            struct iovec _BufferIO[64];
            int32_t      iBufferIOCount = 0;
            while (!QUEUE_EMPTY(&pHandle->queueWritePending)) {
                pNode     = QUEUE_HEAD(&pHandle->queueWritePending);
                pEventBuf = eventConnection_getEventConnectionWrite(pHandle, pNode);

                if (pEventBuf->uiLength & 0x80000000) {
                    int32_t      iCount    = pEventBuf->uiLength & 0x7fffffff;
                    ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;

                    for (int32_t i = 0; i < Min(iCount, 64 - iBufferIOCount); ++i) {
                        _BufferIO[iBufferIOCount].iov_base = pBufWrite[i].pBuf;
                        _BufferIO[iBufferIOCount].iov_len  = pBufWrite[i].iLength;
                        nWriteLength += pBufWrite[i].iLength;
                        ++iBufferIOCount;
                    }
                }
                else {
                    size_t nLength = pEventBuf->uiLength & 0x7fffffff;
                    nWriteLength += nLength;
                    _BufferIO[iBufferIOCount].iov_base = pEventBuf->szStorage;
                    _BufferIO[iBufferIOCount].iov_len  = nLength;
                    ++iBufferIOCount;
                }

                if (iBufferIOCount == 64 || nWriteLength >= g_nSendBufferMaxLength) {
                    break;
                }
            }
            iBytesSent = writev(pHandle->hSocket, _BufferIO, iBufferIOCount);
        }

        if (iBytesSent > 0) {
            pHandle->nWritten += iBytesSent;
        }
        else {
            int32_t iError = errno;
            if (TEST_ERR_RW_RETRIABLE(iError)) {
                DLog(eLog_warning, "Send warning, errno=%d", iError);
            }
            else {
                while (!QUEUE_EMPTY(&pHandle->queueWritePending)) {
                    pNode     = QUEUE_HEAD(&pHandle->queueWritePending);
                    pEventBuf = eventConnection_getEventConnectionWrite(pHandle, pNode);
                    nLength   = pEventBuf->uiLength & 0x7fffffff;
                    QUEUE_REMOVE(pNode);
                    --pHandle->iWritePending;
                    pHandle->nWritePendingBytes -= nLength;
                    if (pEventBuf->fnCallback) {
                        pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                              pEventBuf->pEventConnection->pUserData,
                                              false,
                                              pEventBuf->uiWriteUser);
                    }
                    eventBuf_release(pEventBuf);
                }
                pHandle->nWritten = 0;
                poller_clear(pHandle->pEventIOLoop->pPoller,
                             pHandle->hSocket,
                             &pHandle->pollHandle,
                             ePollerClosed);
                disconnectCallbackPtr fnDisconnectCallback =
                    (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, 0);
                if (fnDisconnectCallback) {
                    fnDisconnectCallback(pHandle, pHandle->pUserData);
                }
            }
        }
    }
    else {
        poller_clear(pHandle->pEventIOLoop->pPoller,
                     pHandle->hSocket,
                     &pHandle->pollHandle,
                     ePollerWritable);
    }
}

static inline void eventConnection_handleEvent(struct pollHandle_s* pHandle, int32_t iAttribute)
{
    eventConnection_tt* pEventConnection = container_of(pHandle, eventConnection_tt, pollHandle);
    switch (atomic_load(&pEventConnection->iStatus)) {
    case eConnected:
    {
        if (iAttribute & ePollerClosed) {
            poller_clear(pEventConnection->pEventIOLoop->pPoller,
                         pEventConnection->hSocket,
                         &pEventConnection->pollHandle,
                         ePollerReadable);
            disconnectCallbackPtr fnDisconnectCallback =
                (disconnectCallbackPtr)atomic_exchange(&pEventConnection->hDisconnectCallback, 0);
            if (fnDisconnectCallback) {
                fnDisconnectCallback(pEventConnection, pEventConnection->pUserData);
            }
        }
        else {
            if (iAttribute & ePollerWritable) {
                eventConnection_sendCompleteCallback(pEventConnection);
            }

            if (iAttribute & ePollerReadable) {
                if (!eventConnection_handleRecv(pEventConnection)) {
                    poller_clear(pEventConnection->pEventIOLoop->pPoller,
                                 pEventConnection->hSocket,
                                 &pEventConnection->pollHandle,
                                 ePollerReadable);
                    disconnectCallbackPtr fnDisconnectCallback =
                        (disconnectCallbackPtr)atomic_exchange(
                            &pEventConnection->hDisconnectCallback, 0);
                    if (fnDisconnectCallback) {
                        fnDisconnectCallback(pEventConnection, pEventConnection->pUserData);
                    }
                }
            }
        }
    } break;
    case eConnecting:
    {
        if (iAttribute & ePollerClosed) {
            atomic_store(&pEventConnection->iStatus, eDisconnected);
            poller_clear(pEventConnection->pEventIOLoop->pPoller,
                         pEventConnection->hSocket,
                         &pEventConnection->pollHandle,
                         ePollerClosed);
            close(pEventConnection->hSocket);
            pEventConnection->hSocket = -1;
            atomic_fetch_sub(&(pEventConnection->pEventIOLoop->iConnections), 1);
            if (pEventConnection->fnConnectorCallback) {
                pEventConnection->fnConnectorCallback(pEventConnection,
                                                      pEventConnection->pUserData);
            }
        }
        else {
            if (iAttribute & ePollerWritable) {
                int32_t   iError = 0;
                socklen_t nLen   = sizeof(iError);
                int32_t   r      = getsockopt(pEventConnection->hSocket,
                                       SOL_SOCKET,
                                       SO_ERROR,
                                       (char*)&iError,
                                       (socklen_t*)&nLen);
                if (r < 0 || iError) {
                    atomic_store(&pEventConnection->iStatus, eDisconnected);
                    poller_clear(pEventConnection->pEventIOLoop->pPoller,
                                 pEventConnection->hSocket,
                                 &pEventConnection->pollHandle,
                                 ePollerClosed);
                    close(pEventConnection->hSocket);
                    atomic_fetch_sub(&(pEventConnection->pEventIOLoop->iConnections), 1);
                    pEventConnection->hSocket = -1;
                }
                else {
                    poller_clear(pEventConnection->pEventIOLoop->pPoller,
                                 pEventConnection->hSocket,
                                 &pEventConnection->pollHandle,
                                 ePollerWritable);
                }

                if (pEventConnection->fnConnectorCallback) {
                    pEventConnection->fnConnectorCallback(pEventConnection,
                                                          pEventConnection->pUserData);
                }
            }
        }
        eventConnection_release(pEventConnection);
    } break;
    case eDisconnecting:
    {
        if (iAttribute & ePollerClosed) {
            eventConnection_handleClose(pEventConnection);
        }
        else if (iAttribute & ePollerReadable) {
            if (!eventConnection_handleRecv(pEventConnection)) {
                eventConnection_handleClose(pEventConnection);
            }
        }
    } break;
    }
}

typedef struct eventConnectionAsync_s
{
    eventConnection_tt* pEventConnection;
    eventAsync_tt       eventAsync;
} eventConnectionAsync_tt;

static inline void inLoop_eventConnection_cancel(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;
    eventConnection_release(pHandle);
    mem_free(pEventConnectionAsync);
}

static inline void inLoop_eventConnection_bind(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;
    if (pHandle->bTcp) {
        setKeepAlive(pHandle->hSocket, pHandle->bKeepAlive);
        setTcpNoDelay(pHandle->hSocket, pHandle->bTcpNoDelay);
    }

    if (pollHandle_isClosed(&pHandle->pollHandle)) {
        poller_add(pHandle->pEventIOLoop->pPoller,
                   pHandle->hSocket,
                   &pHandle->pollHandle,
                   ePollerReadable,
                   eventConnection_handleEvent);
    }
    else {
        poller_setOpt(pHandle->pEventIOLoop->pPoller,
                      pHandle->hSocket,
                      &pHandle->pollHandle,
                      ePollerReadable);
    }

    atomic_store(&pHandle->iStatus, eConnected);
    mem_free(pEventConnectionAsync);
}

static inline void inLoop_eventConnection_connect(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;

    if (pHandle->bTcp) {
        pHandle->hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (pHandle->hSocket < 0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        setSocketNonblocking(pHandle->hSocket);

        struct sockaddr_in localAddress;
        memset(&localAddress, 0, sizeof(struct sockaddr_in));
        localAddress.sin_family      = AF_INET;
        localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddress.sin_port        = htons(0);

        if (bind(pHandle->hSocket, (struct sockaddr*)&localAddress, sizeof(struct sockaddr_in)) <
            0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            close(pHandle->hSocket);
            pHandle->hSocket = -1;
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        if (connect(pHandle->hSocket,
                    inetAddress_getSockaddr(&pHandle->remoteAddr),
                    inetAddress_getSocklen(&pHandle->remoteAddr)) == -1) {
            int32_t iError = errno;
            if (!TEST_ERR_CONNECT_RETRIABLE(iError)) {
                atomic_store(&pHandle->iStatus, eDisconnected);
                close(pHandle->hSocket);
                pHandle->hSocket = -1;
                if (pHandle->fnConnectorCallback) {
                    pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
                }
                atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
                eventConnection_release(pHandle);
                mem_free(pEventConnectionAsync);
                return;
            }
        }

        socklen_t addrlen = sizeof pHandle->localAddr;
        if (getsockname(pHandle->hSocket, inetAddress_getSockaddr(&pHandle->localAddr), &addrlen) <
            0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            close(pHandle->hSocket);
            pHandle->hSocket = -1;
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        poller_add(pHandle->pEventIOLoop->pPoller,
                   pHandle->hSocket,
                   &pHandle->pollHandle,
                   ePollerWritable,
                   eventConnection_handleEvent);
    }
    else {
        pHandle->hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (pHandle->hSocket < 0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        setSocketNonblocking(pHandle->hSocket);

        struct sockaddr_in localAddress;
        memset(&localAddress, 0, sizeof(struct sockaddr_in));
        localAddress.sin_family      = AF_INET;
        localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddress.sin_port        = htons(0);

        if (bind(pHandle->hSocket, (struct sockaddr*)&localAddress, sizeof(struct sockaddr_in)) <
            0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            close(pHandle->hSocket);
            pHandle->hSocket = -1;
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        if (connect(pHandle->hSocket,
                    inetAddress_getSockaddr(&pHandle->remoteAddr),
                    inetAddress_getSocklen(&pHandle->remoteAddr)) == -1) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            close(pHandle->hSocket);
            pHandle->hSocket = -1;
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        socklen_t addrlen = sizeof pHandle->localAddr;
        if (getsockname(pHandle->hSocket, inetAddress_getSockaddr(&pHandle->localAddr), &addrlen) <
            0) {
            atomic_store(&pHandle->iStatus, eDisconnected);
            close(pHandle->hSocket);
            pHandle->hSocket = -1;
            if (pHandle->fnConnectorCallback) {
                pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
            }
            atomic_fetch_sub(&(pHandle->pEventIOLoop->iConnections), 1);
            eventConnection_release(pHandle);
            mem_free(pEventConnectionAsync);
            return;
        }

        if (pHandle->fnConnectorCallback) {
            pHandle->fnConnectorCallback(pHandle, pHandle->pUserData);
        }
        eventConnection_release(pHandle);
    }
    mem_free(pEventConnectionAsync);
}

static inline void inLoop_eventConnection_forceClose(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;
    eventConnection_handleClose(pHandle);
    eventConnection_release(pHandle);
    mem_free(pEventConnectionAsync);
}

static inline void inLoop_eventConnection_close(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;
    if (pHandle->bTcp) {
        if (pollHandle_isReading(&pHandle->pollHandle)) {
            if (shutdown(pHandle->hSocket, SHUTDOWN_WR) < 0) {
                eventConnection_handleClose(pHandle);
            }
        }
        else {
            eventConnection_handleClose(pHandle);
        }
    }
    else {
        eventConnection_handleClose(pHandle);
    }

    eventConnection_release(pHandle);
    mem_free(pEventConnectionAsync);
}

static inline void inLoop_eventConnection_resetRecv(eventAsync_tt* pEventAsync)
{
    eventConnectionAsync_tt* pEventConnectionAsync =
        container_of(pEventAsync, eventConnectionAsync_tt, eventAsync);
    eventConnection_tt* pHandle = pEventConnectionAsync->pEventConnection;

    if (atomic_load(&pHandle->iStatus) == eConnected &&
        !pollHandle_isClosed(&pHandle->pollHandle)) {
        poller_setOpt(pHandle->pEventIOLoop->pPoller,
                      pHandle->hSocket,
                      &pHandle->pollHandle,
                      ePollerReadable);
    }
    else {
        eventConnection_release(pHandle);
    }

    mem_free(pEventConnectionAsync);
}

static inline int32_t eventConnection_sendData(eventConnection_tt* pHandle, eventBuf_tt* pEventBuf)
{
    int32_t iWritten = 0;
    if (!pollHandle_isWriting(&pHandle->pollHandle)) {
        size_t  nRemaining = 0;
        int32_t iLength    = 0;
        if (pEventBuf->uiLength & 0x80000000) {
            const int32_t iCount    = pEventBuf->uiLength & 0x7fffffff;
            ioBufVec_tt*  pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
            struct iovec  _BufferIO[iCount];
            for (int32_t i = 0; i < iCount; ++i) {
                _BufferIO[i].iov_base = pBufWrite[i].pBuf;
                _BufferIO[i].iov_len  = pBufWrite[i].iLength;
                iLength += pBufWrite[i].iLength;
            }

            iWritten = writev(pHandle->hSocket, _BufferIO, iCount);
        }
        else {
            iLength  = pEventBuf->uiLength & 0x7fffffff;
            iWritten = send(pHandle->hSocket, pEventBuf->szStorage, iLength, MSG_NOSIGNAL);
        }

        if (iWritten >= 0) {
            nRemaining = iLength - iWritten;
        }
        else {
            int32_t iError = errno;
            if (TEST_ERR_RW_RETRIABLE(iError)) {
                DLog(eLog_warning, "Send warning, errno=%d", iError);
            }
            else {
                poller_clear(pHandle->pEventIOLoop->pPoller,
                             pHandle->hSocket,
                             &pHandle->pollHandle,
                             ePollerClosed);
                if (pEventBuf->fnCallback) {
                    pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                          pEventBuf->pEventConnection->pUserData,
                                          false,
                                          pEventBuf->uiWriteUser);
                }
                eventBuf_release(pEventBuf);
                return -1;
            }
        }

        if (nRemaining > 0) {
            ++pHandle->iWritePending;
            pHandle->nWritten = iWritten;
            pHandle->nWritePendingBytes += iLength;
            eventConnection_insertQueueWritePending(pHandle, &(pEventBuf->eventAsync));
            poller_setOpt(pHandle->pEventIOLoop->pPoller,
                          pHandle->hSocket,
                          &pHandle->pollHandle,
                          ePollerWritable);
        }
        else {
            if (pEventBuf->fnCallback) {
                pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                      pEventBuf->pEventConnection->pUserData,
                                      true,
                                      pEventBuf->uiWriteUser);
            }
            eventBuf_release(pEventBuf);
        }
    }
    else {
        ++pHandle->iWritePending;
        if (pEventBuf->uiLength & 0x80000000) {
            int32_t      iCount    = pEventBuf->uiLength & 0x7fffffff;
            ioBufVec_tt* pBufWrite = (ioBufVec_tt*)pEventBuf->szStorage;
            for (int32_t i = 0; i < iCount; ++i) {
                iWritten += pBufWrite[i].iLength;
            }
        }
        else {
            iWritten = pEventBuf->uiLength & 0x7fffffff;
        }
        pHandle->nWritePendingBytes += iWritten;
        eventConnection_insertQueueWritePending(pHandle, &(pEventBuf->eventAsync));
    }
    return iWritten;
}

static inline void eventAsyncSend_sendInLoop(eventAsync_tt* pEventAsync)
{
    eventBuf_tt*        pEventBuf = container_of(pEventAsync, eventBuf_tt, eventAsync);
    eventConnection_tt* pHandle   = pEventBuf->pEventConnection;
    if (atomic_load(&pHandle->iStatus) == eConnected) {
        int32_t iWritten = eventConnection_sendData(pHandle, pEventBuf);
        if (iWritten == -1) {
            disconnectCallbackPtr fnDisconnectCallback =
                (disconnectCallbackPtr)atomic_exchange(&pHandle->hDisconnectCallback, 0);
            if (fnDisconnectCallback) {
                fnDisconnectCallback(pHandle, pHandle->pUserData);
            }
        }
    }
    else {
        if (pEventBuf->fnCallback) {
            pEventBuf->fnCallback(pEventBuf->pEventConnection,
                                  pEventBuf->pEventConnection->pUserData,
                                  false,
                                  pEventBuf->uiWriteUser);
        }
        eventBuf_release(pEventBuf);
    }
    eventConnection_release(pHandle);
}

static inline void eventAsyncSend_sendCancel(eventAsync_tt* pEventAsync)
{
    eventBuf_tt*        pEventBuf = container_of(pEventAsync, eventBuf_tt, eventAsync);
    eventConnection_tt* pHandle   = pEventBuf->pEventConnection;
    if (pEventBuf->fnCallback) {
        pEventBuf->fnCallback(pEventBuf->pEventConnection,
                              pEventBuf->pEventConnection->pUserData,
                              false,
                              pEventBuf->uiWriteUser);
    }
    eventBuf_release(pEventBuf);
    eventConnection_release(pHandle);
}

eventConnection_tt* createEventConnection(eventIO_tt* pEventIO, const inetAddress_tt* pInetAddress,
                                          bool bTcp)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIOLoop        = eventIO_connectionLoop(pEventIO);
    pHandle->remoteAddr          = *pInetAddress;
    pHandle->bTcp                = bTcp;
    pHandle->pListenPort         = NULL;
    pHandle->fnUserFree          = NULL;
    pHandle->pUserData           = NULL;
    pHandle->fnConnectorCallback = NULL;
    pHandle->fnReceiveCallback   = NULL;
    pHandle->fnCloseCallback     = NULL;
    atomic_init(&pHandle->hDisconnectCallback, 0);
    pHandle->hSocket     = -1;
    pHandle->bKeepAlive  = false;
    pHandle->bTcpNoDelay = false;
    pollHandle_init(&pHandle->pollHandle);

    if (bTcp) {
        byteQueue_init(&pHandle->readByteQueue, 256);
    }
    else {
        byteQueue_init(&pHandle->readByteQueue, MAXIMUM_MTU_SIZE);
    }

    QUEUE_INIT(&pHandle->queueWritePending);
    pHandle->nWritten           = 0;
    pHandle->iWritePending      = 0;
    pHandle->nWritePendingBytes = 0;
    atomic_init(&pHandle->iStatus, eDisconnected);
    atomic_init(&pHandle->iRefCount, 1);

    return pHandle;
}

struct eventConnection_s* acceptEventConnection(struct eventIO_s*         pEventIO,
                                                struct eventListenPort_s* pListenPort,
                                                int32_t hSocket, const inetAddress_tt* pRemoteAddr,
                                                const inetAddress_tt* pLocalAddr)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIOLoop        = eventIO_connectionLoop(pEventIO);
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
    pHandle->bKeepAlive  = false;
    pHandle->bTcpNoDelay = false;
    pHandle->bTcp        = true;

    pollHandle_init(&pHandle->pollHandle);
    byteQueue_init(&pHandle->readByteQueue, 256);
    QUEUE_INIT(&pHandle->queueWritePending);
    pHandle->nWritten           = 0;
    pHandle->iWritePending      = 0;
    pHandle->nWritePendingBytes = 0;
    atomic_init(&pHandle->iStatus, eConnecting);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

struct eventConnection_s* acceptUdpEventConnection(struct eventIO_s*         pEventIO,
                                                   struct eventListenPort_s* pListenPort,
                                                   int32_t                   hSocket,
                                                   const inetAddress_tt*     pRemoteAddr,
                                                   const inetAddress_tt*     pLocalAddr)
{
    eventConnection_tt* pHandle  = (eventConnection_tt*)mem_malloc(sizeof(eventConnection_tt));
    pHandle->pEventIOLoop        = eventIO_connectionLoop(pEventIO);
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
    pHandle->bKeepAlive  = false;
    pHandle->bTcpNoDelay = false;
    pHandle->bTcp        = false;

    pollHandle_init(&pHandle->pollHandle);
    byteQueue_init(&pHandle->readByteQueue, MAXIMUM_MTU_SIZE);
    QUEUE_INIT(&pHandle->queueWritePending);
    pHandle->nWritten           = 0;
    pHandle->iWritePending      = 0;
    pHandle->nWritePendingBytes = 0;
    atomic_init(&pHandle->iStatus, eConnecting);
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
        byteQueue_clear(&(pHandle->readByteQueue));

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
        pHandle->bKeepAlive  = bKeepAlive;
        pHandle->bTcpNoDelay = bTcpNoDelay;
        eventConnectionAsync_tt* pEventConnectionAsync =
            mem_malloc(sizeof(eventConnectionAsync_tt));
        pEventConnectionAsync->pEventConnection = pHandle;
        eventConnection_addref(pEventConnectionAsync->pEventConnection);
        eventIOLoop_runInLoop(pHandle->pEventIOLoop,
                              &pEventConnectionAsync->eventAsync,
                              inLoop_eventConnection_bind,
                              inLoop_eventConnection_cancel);
        return true;
    }
    return false;
}

bool eventConnection_connect(eventConnection_tt* pHandle, void* pUserData,
                             void (*fnUserFree)(void*))
{
    int32_t iStatus = eDisconnected;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eConnecting)) {
        if (pUserData) {
            if (pHandle->fnUserFree && pHandle->pUserData) {
                pHandle->fnUserFree(pHandle->pUserData);
            }
            pHandle->pUserData  = pUserData;
            pHandle->fnUserFree = fnUserFree;
        }

        eventConnectionAsync_tt* pEventConnectionAsync =
            mem_malloc(sizeof(eventConnectionAsync_tt));
        pEventConnectionAsync->pEventConnection = pHandle;
        eventConnection_addref(pEventConnectionAsync->pEventConnection);
        eventIOLoop_runInLoop(pHandle->pEventIOLoop,
                              &pEventConnectionAsync->eventAsync,
                              inLoop_eventConnection_connect,
                              inLoop_eventConnection_cancel);
        return true;
    }
    return false;
}

void eventConnection_close(eventConnection_tt* pHandle)
{
    int32_t iStatus = eConnected;
    if (atomic_compare_exchange_strong(&pHandle->iStatus, &iStatus, eDisconnecting)) {
        eventConnectionAsync_tt* pEventConnectionAsync =
            mem_malloc(sizeof(eventConnectionAsync_tt));
        pEventConnectionAsync->pEventConnection = pHandle;
        atomic_fetch_add(&(pHandle->iRefCount), 1);
        eventIOLoop_queueInLoop(pHandle->pEventIOLoop,
                                &pEventConnectionAsync->eventAsync,
                                inLoop_eventConnection_close,
                                inLoop_eventConnection_forceClose);
    }
}

void eventConnection_forceClose(eventConnection_tt* pHandle)
{
    int32_t iStatus = atomic_exchange(&pHandle->iStatus, eDisconnected);
    if (iStatus != eDisconnected) {
        eventConnectionAsync_tt* pEventConnectionAsync =
            mem_malloc(sizeof(eventConnectionAsync_tt));
        pEventConnectionAsync->pEventConnection = pHandle;
        atomic_fetch_add(&(pHandle->iRefCount), 1);
        eventIOLoop_queueInLoop(pHandle->pEventIOLoop,
                                &pEventConnectionAsync->eventAsync,
                                inLoop_eventConnection_forceClose,
                                inLoop_eventConnection_forceClose);
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

int32_t eventConnection_send(eventConnection_tt* pHandle, eventBuf_tt* pEventBuf)
{
    if (atomic_load(&pHandle->iStatus) != eConnected) {
        return -1;
    }

    if (eventIOLoop_isInLoopThread(pHandle->pEventIOLoop)) {
        return eventConnection_sendData(pHandle, pEventBuf);
    }
    else {
        pEventBuf->pEventConnection = pHandle;
        eventConnection_addref(pEventBuf->pEventConnection);
        eventIOLoop_runInLoop(pHandle->pEventIOLoop,
                              &pEventBuf->eventAsync,
                              eventAsyncSend_sendInLoop,
                              eventAsyncSend_sendCancel);
    }
    return 0;
}

int32_t eventConnection_getWritePending(eventConnection_tt* pHandle)
{
    return pHandle->iWritePending;
}

size_t eventConnection_getWritePendingBytes(eventConnection_tt* pHandle)
{
    return pHandle->nWritePendingBytes;
}

size_t eventConnection_getReceiveBufLength(eventConnection_tt* pHandle)
{
    return byteQueue_getCapacity(&pHandle->readByteQueue);
}
