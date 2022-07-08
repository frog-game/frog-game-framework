#include "eventIO/internal/posix/eventListenPort_t.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>

#if defined(__FreeBSD__) && __FreeBSD__ >= 10
#    define def_ACCEPT4 1
#endif

#include "log_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/internal/posix/eventIO-inl.h"
#include "eventIO/internal/posix/eventConnection_t.h"

#define TEST_ERR_ACCEPT_RETRIABLE(e) ((e) == EINTR || (e) == EAGAIN || (e) == ECONNABORTED)

#define TEST_ERR_CONNECT_RETRIABLE(e) ((e) == EINTR || (e) == EINPROGRESS)

static const size_t g_nRecvBufferMaxLength = 65536;

static bool (*s_fnRecvFromFilterCallback)(const inetAddress_tt*, const char*, uint32_t);

static inline void setNonBlockAndCloseOnExec(int32_t hSocket)
{
    int32_t iFlags = fcntl(hSocket, F_GETFL, 0);
    fcntl(hSocket, F_SETFL, iFlags | O_NONBLOCK);
    iFlags = fcntl(hSocket, F_GETFD, 0);
    fcntl(hSocket, F_SETFD, iFlags | FD_CLOEXEC);
}

static inline struct sockaddr_in6 getLocalAddr(int32_t hSocket)
{
    struct sockaddr_in6 localaddr;
    bzero(&localaddr, sizeof localaddr);
    socklen_t addrlen = (socklen_t)(sizeof localaddr);
    if (getsockname(hSocket, (struct sockaddr*)&localaddr, &addrlen) < 0) {
        int32_t iError = errno;
        Log(eLog_error, "getLocalAddr, errno=%d", iError);
    }
    return localaddr;
}

static inline void setReuseAddr(int32_t hSocket)
{
#ifdef SO_REUSEADDR
    int32_t iOptval = 1;
    int32_t rc      = setsockopt(
        hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (rc != 0) {
        int32_t iError = errno;
        Log(eLog_error, "setsockopt(SO_REUSEADDR), errno==%d", iError);
    }
#endif
}

static inline void setReusePort(int32_t hSocket)
{
#ifdef SO_REUSEPORT
    int32_t iOptval = 1;
    int32_t rc      = setsockopt(
        hSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (rc != 0) {
        int32_t iError = errno;
        Log(eLog_error, "setsockopt(SO_REUSEPORT), errno==%d", iError);
    }
#endif
}

static inline void setRecvBuf(int32_t hSocket, int32_t iRecvBuf)
{
    int32_t rc = setsockopt(
        hSocket, SOL_SOCKET, SO_RCVBUF, (const char*)(&iRecvBuf), (socklen_t)(sizeof iRecvBuf));
    if (rc != 0) {
        int32_t iError = errno;
        Log(eLog_error, "setsockopt(SO_REUSEPORT), errno==%d", iError);
    }
}

static void tls_recvBuffer_cleanup_func(void* pArg)
{
    mem_free(pArg);
}

static void tls_addressConnection_cleanup_func(void* pArg)
{
    mem_free(pArg);
}

static inline void eventListenPort_handleEvent(struct pollHandle_s* pHandle, int32_t iAttribute)
{
    listenHandle_tt*    pListenHandle    = container_of(pHandle, listenHandle_tt, pollHandle);
    eventListenPort_tt* pEventListenPort = pListenHandle->pEventListenPort;

    inetAddress_tt remoteAddr;
    socklen_t      addrlen = sizeof(inetAddress_tt);
    bzero(&remoteAddr, sizeof(inetAddress_tt));

    if (iAttribute & ePollerReadable) {
        if (pEventListenPort->bTcp) {
            // for(;;)
            // {
#ifdef def_ACCEPT4
            int32_t hConnectSocket = accept4(pListenHandle->hSocket,
                                             inetAddress_getSockaddr(&remoteAddr),
                                             &addrlen,
                                             SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
            int32_t hConnectSocket =
                accept(pListenHandle->hSocket, inetAddress_getSockaddr(&remoteAddr), &addrlen);
#endif
            if (hConnectSocket != -1) {
#ifndef def_ACCEPT4
                setNonBlockAndCloseOnExec(hConnectSocket);
#endif
                inetAddress_tt localAddr;
                inetAddress_init_V6(&localAddr, getLocalAddr(hConnectSocket));
                eventConnection_tt* pEventConnection =
                    acceptEventConnection(pEventListenPort->pEventIO,
                                          pEventListenPort,
                                          hConnectSocket,
                                          &remoteAddr,
                                          &localAddr);
                if (pEventListenPort->fnAcceptCallback) {
                    pEventListenPort->fnAcceptCallback(
                        pEventListenPort, pEventConnection, NULL, 0, pEventListenPort->pUserData);
                }
            }
            else {
                int32_t iError = errno;
                if (!TEST_ERR_ACCEPT_RETRIABLE(iError)) {
                    Log(eLog_error, "accept errno:%d", iError);
                }
                Log(eLog_error, "accept errno>>>:%d", iError);
            }
            // }
        }
        else {
            static _decl_threadLocal char* s_pRecvBuffer = NULL;
            if (s_pRecvBuffer == NULL) {
                s_pRecvBuffer = (char*)mem_malloc(g_nRecvBufferMaxLength);
                setTlsValue(pHandle, tls_recvBuffer_cleanup_func, s_pRecvBuffer, true);
            }

            static _decl_threadLocal addressConnection_tt* s_pFindAddressConnection = NULL;
            const int32_t                                  iFlag                    = 0;

            // for(;;)
            // {

            int32_t iBytesRead = recvfrom(pListenHandle->hSocket,
                                          s_pRecvBuffer,
                                          g_nRecvBufferMaxLength,
                                          iFlag,
                                          inetAddress_getSockaddr(&remoteAddr),
                                          &addrlen);
            if (iBytesRead > 0) {
                if (s_pFindAddressConnection == NULL) {
                    s_pFindAddressConnection = mem_malloc(sizeof(addressConnection_tt));
                    setTlsValue(pEventListenPort,
                                tls_addressConnection_cleanup_func,
                                s_pFindAddressConnection,
                                true);
                }

                s_pFindAddressConnection->inetAddress = remoteAddr;
                s_pFindAddressConnection->hash        = inetAddress_hash(&remoteAddr);

                rwSpinLock_rdlock(&pEventListenPort->rwlock);
                addressConnection_tt* pNode = RB_FIND(addressConnectionMap_s,
                                                      &pEventListenPort->mapAddressConnection,
                                                      s_pFindAddressConnection);
                rwSpinLock_rdunlock(&pEventListenPort->rwlock);
                if (pNode == NULL) {
                    if (s_fnRecvFromFilterCallback) {
                        if (!s_fnRecvFromFilterCallback(&remoteAddr, s_pRecvBuffer, iBytesRead)) {
                            return;
                            // continue;
                        }
                    }

                    rwSpinLock_wrlock(&pEventListenPort->rwlock);
                    pNode = RB_INSERT(addressConnectionMap_s,
                                      &pEventListenPort->mapAddressConnection,
                                      s_pFindAddressConnection);
                    rwSpinLock_wrunlock(&pEventListenPort->rwlock);
                    if (pNode == NULL) {
                        int32_t hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                        if (hSocket < 0) {
                            rwSpinLock_wrlock(&pEventListenPort->rwlock);
                            RB_REMOVE(addressConnectionMap_s,
                                      &pEventListenPort->mapAddressConnection,
                                      s_pFindAddressConnection);
                            rwSpinLock_wrunlock(&pEventListenPort->rwlock);
                            return;
                            // continue;
                        }

                        setSocketNonblocking(hSocket);
                        setReuseAddr(hSocket);
                        setReusePort(hSocket);

                        if (bind(hSocket,
                                 inetAddress_getSockaddr(&pEventListenPort->listenAddr),
                                 inetAddress_getSocklen(&pEventListenPort->listenAddr)) < 0) {
                            rwSpinLock_wrlock(&pEventListenPort->rwlock);
                            RB_REMOVE(addressConnectionMap_s,
                                      &pEventListenPort->mapAddressConnection,
                                      s_pFindAddressConnection);
                            rwSpinLock_wrunlock(&pEventListenPort->rwlock);
                            close(hSocket);
                            return;
                            // continue;
                        }

                        if (connect(hSocket,
                                    inetAddress_getSockaddr(&remoteAddr),
                                    inetAddress_getSocklen(&remoteAddr)) == -1) {
                            int32_t iError = errno;
                            if (!TEST_ERR_CONNECT_RETRIABLE(iError)) {
                                rwSpinLock_wrlock(&pEventListenPort->rwlock);
                                RB_REMOVE(addressConnectionMap_s,
                                          &pEventListenPort->mapAddressConnection,
                                          s_pFindAddressConnection);
                                rwSpinLock_wrunlock(&pEventListenPort->rwlock);
                                close(hSocket);
                                return;
                                // continue;
                            }
                        }

                        eventConnection_tt* pEventConnection =
                            acceptUdpEventConnection(pEventListenPort->pEventIO,
                                                     pEventListenPort,
                                                     hSocket,
                                                     &remoteAddr,
                                                     &pEventListenPort->listenAddr);
                        s_pFindAddressConnection = NULL;

                        if (pEventListenPort->fnAcceptCallback) {
                            pEventListenPort->fnAcceptCallback(pEventListenPort,
                                                               pEventConnection,
                                                               s_pRecvBuffer,
                                                               iBytesRead,
                                                               pEventListenPort->pUserData);
                        }

                        s_pFindAddressConnection = mem_malloc(sizeof(addressConnection_tt));
                        setTlsValue(pEventListenPort,
                                    tls_addressConnection_cleanup_func,
                                    s_pFindAddressConnection,
                                    false);
                    }
                }
            }
            else {
                int32_t iError = errno;
                Log(eLog_error, "recvfrom errno:%d", iError);
                // break;
            }
            // }
        }
    }
}

typedef struct eventListenPortAsync_s
{
    eventAsync_tt    eventAsync;
    listenHandle_tt* pListenHandle;
} eventListenPortAsync_tt;

static inline void inLoop_eventListenPort_cancel(eventAsync_tt* pEventAsync)
{
    eventListenPortAsync_tt* pEventListenPortAsync =
        container_of(pEventAsync, eventListenPortAsync_tt, eventAsync);
    listenHandle_tt* pListenHandle = pEventListenPortAsync->pListenHandle;
    eventListenPort_release(pListenHandle->pEventListenPort);
    pListenHandle->pEventListenPort = NULL;
    mem_free(pEventListenPortAsync);
}

static inline void inLoop_eventListenPort_postAccept(eventAsync_tt* pEventAsync)
{
    eventListenPortAsync_tt* pEventListenPortAsync =
        container_of(pEventAsync, eventListenPortAsync_tt, eventAsync);
    listenHandle_tt*    pListenHandle = pEventListenPortAsync->pListenHandle;
    eventIOLoop_tt*     pEventIOLoop  = pListenHandle->pEventIOLoop;
    eventListenPort_tt* pHandle       = pListenHandle->pEventListenPort;
    if (pHandle->bTcp) {
        pListenHandle->hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (pListenHandle->hSocket < 0) {
            pListenHandle->pEventListenPort = NULL;
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            eventListenPort_release(pHandle);
            mem_free(pEventListenPortAsync);
            return;
        }
        setSocketNonblocking(pListenHandle->hSocket);
        setReusePort(pListenHandle->hSocket);

        if (bind(pListenHandle->hSocket,
                 inetAddress_getSockaddr(&pHandle->listenAddr),
                 inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
            close(pListenHandle->hSocket);
            pListenHandle->hSocket          = -1;
            pListenHandle->pEventListenPort = NULL;

            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            eventListenPort_release(pHandle);
            mem_free(pEventListenPortAsync);
            return;
        }
        listen(pListenHandle->hSocket, SOMAXCONN);
    }
    else {
        pListenHandle->hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (pListenHandle->hSocket < 0) {
            pListenHandle->pEventListenPort = NULL;
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            eventListenPort_release(pHandle);
            mem_free(pEventListenPortAsync);
            return;
        }
        setRecvBuf(pListenHandle->hSocket, 8 * 1024 * 1024);
        setSocketNonblocking(pListenHandle->hSocket);
        setReuseAddr(pListenHandle->hSocket);
        setReusePort(pListenHandle->hSocket);

        if (bind(pListenHandle->hSocket,
                 inetAddress_getSockaddr(&pHandle->listenAddr),
                 inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
            close(pListenHandle->hSocket);
            pListenHandle->hSocket          = -1;
            pListenHandle->pEventListenPort = NULL;
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            eventListenPort_release(pHandle);
            mem_free(pEventListenPortAsync);
            return;
        }
    }
    poller_add(pEventIOLoop->pPoller,
               pListenHandle->hSocket,
               &pListenHandle->pollHandle,
               ePollerReadable,
               eventListenPort_handleEvent);
    mem_free(pEventListenPortAsync);
}

static inline void inLoop_eventListenPort_close(eventAsync_tt* pEventAsync)
{
    eventListenPortAsync_tt* pEventListenPortAsync =
        container_of(pEventAsync, eventListenPortAsync_tt, eventAsync);
    listenHandle_tt*    pListenHandle    = pEventListenPortAsync->pListenHandle;
    eventIOLoop_tt*     pEventIOLoop     = pListenHandle->pEventIOLoop;
    eventListenPort_tt* pEventListenPort = pListenHandle->pEventListenPort;
    if (pListenHandle->hSocket != -1) {
        poller_clear(pEventIOLoop->pPoller,
                     pListenHandle->hSocket,
                     &pListenHandle->pollHandle,
                     ePollerClosed);
        close(pListenHandle->hSocket);
        pListenHandle->hSocket = -1;
    }

    if (pListenHandle->pEventListenPort) {
        eventListenPort_release(pListenHandle->pEventListenPort);
        pListenHandle->pEventListenPort = NULL;
    }
    eventListenPort_release(pEventListenPort);
    mem_free(pEventListenPortAsync);
}

void eventListenPort_removeAddressConnection(struct eventListenPort_s* pHandle,
                                             const inetAddress_tt*     pRemoteAddr)
{
    addressConnection_tt findNode;
    findNode.hash        = inetAddress_hash(pRemoteAddr);
    findNode.inetAddress = *pRemoteAddr;
    rwSpinLock_wrlock(&pHandle->rwlock);
    addressConnection_tt* pNode =
        RB_FIND(addressConnectionMap_s, &pHandle->mapAddressConnection, &findNode);
    assert(pNode);
    RB_REMOVE(addressConnectionMap_s, &pHandle->mapAddressConnection, pNode);
    rwSpinLock_wrunlock(&pHandle->rwlock);
    mem_free(pNode);
}

eventListenPort_tt* createEventListenPort(eventIO_tt* pEventIO, const inetAddress_tt* pInetAddress,
                                          bool bTcp)
{
    eventListenPort_tt* pHandle = (eventListenPort_tt*)mem_malloc(sizeof(eventListenPort_tt));
    pHandle->pEventIO           = pEventIO;
    pHandle->bTcp               = bTcp;
    pHandle->pUserData          = NULL;
    pHandle->fnUserFree         = NULL;
    pHandle->fnAcceptCallback   = NULL;
    pHandle->pListenHandle      = NULL;
    pHandle->listenAddr         = *pInetAddress;
    RB_INIT(&pHandle->mapAddressConnection);
    rwSpinLock_init(&pHandle->rwlock);
    atomic_init(&pHandle->bActive, false);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void eventListenPort_setAcceptCallback(eventListenPort_tt* pHandle,
                                       void (*fn)(eventListenPort_tt*, eventConnection_tt*,
                                                  const char*, uint32_t, void*))
{
    pHandle->fnAcceptCallback = fn;
}

void eventListenPort_addref(eventListenPort_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void eventListenPort_release(eventListenPort_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->fnUserFree) {
            pHandle->fnUserFree(pHandle->pUserData);
        }

        if (pHandle->pListenHandle) {
            mem_free(pHandle->pListenHandle);
            pHandle->pListenHandle = NULL;
        }
        mem_free(pHandle);
    }
}

bool eventListenPort_start(eventListenPort_tt* pHandle, void* pUserData, void (*fnUserFree)(void*))
{
    bool bActive = false;
    if (atomic_compare_exchange_strong(&pHandle->bActive, &bActive, true)) {
        if (pHandle->fnUserFree && pHandle->pUserData) {
            pHandle->fnUserFree(pHandle->pUserData);
        }
        pHandle->pUserData  = pUserData;
        pHandle->fnUserFree = fnUserFree;

        if (pHandle->bTcp) {
            int32_t hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (hSocket < 0) {
                atomic_store(&pHandle->bActive, false);
                return false;
            }

            setSocketNonblocking(hSocket);
            setReusePort(hSocket);

            if (bind(hSocket,
                     inetAddress_getSockaddr(&pHandle->listenAddr),
                     inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
                atomic_store(&pHandle->bActive, false);
                close(hSocket);
                return false;
            }
            close(hSocket);
        }
        else {
            int32_t hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (hSocket < 0) {
                atomic_store(&pHandle->bActive, false);
                return false;
            }

            setRecvBuf(hSocket, 8 * 1024 * 1024);
            setSocketNonblocking(hSocket);
            setReuseAddr(hSocket);
            setReusePort(hSocket);

            if (bind(hSocket,
                     inetAddress_getSockaddr(&pHandle->listenAddr),
                     inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
                atomic_store(&pHandle->bActive, false);
                close(hSocket);
                return false;
            }
            close(hSocket);
        }

        if (pHandle->pEventIO->uiCocurrentThreads == 0) {
            pHandle->pListenHandle = mem_malloc(sizeof(listenHandle_tt));
            atomic_fetch_add(&pHandle->iRefCount, 1);
            pHandle->pListenHandle->pEventListenPort = pHandle;
            pollHandle_init(&pHandle->pListenHandle->pollHandle);
            pHandle->pListenHandle->pEventIOLoop = pHandle->pEventIO->pEventIOLoop;
            pHandle->pListenHandle->hSocket      = -1;
            eventListenPortAsync_tt* pEventListenPortAsync =
                mem_malloc(sizeof(eventListenPortAsync_tt));
            pEventListenPortAsync->pListenHandle = pHandle->pListenHandle;
            eventIOLoop_queueInLoop(pHandle->pEventIO->pEventIOLoop,
                                    &pEventListenPortAsync->eventAsync,
                                    inLoop_eventListenPort_postAccept,
                                    inLoop_eventListenPort_cancel);
        }
        else {
            pHandle->pListenHandle =
                mem_malloc(sizeof(listenHandle_tt) * pHandle->pEventIO->uiCocurrentThreads);
            for (uint32_t i = 0; i < pHandle->pEventIO->uiCocurrentThreads; ++i) {
                atomic_fetch_add(&pHandle->iRefCount, 1);
                pHandle->pListenHandle[i].pEventListenPort = pHandle;
                pollHandle_init(&pHandle->pListenHandle[i].pollHandle);
                pHandle->pListenHandle[i].pEventIOLoop = &(pHandle->pEventIO->pEventIOLoop[i]);
                pHandle->pListenHandle[i].hSocket      = -1;
                eventListenPortAsync_tt* pEventListenPortAsync =
                    mem_malloc(sizeof(eventListenPortAsync_tt));
                pEventListenPortAsync->pListenHandle = &(pHandle->pListenHandle[i]);
                eventIOLoop_queueInLoop(&(pHandle->pEventIO->pEventIOLoop[i]),
                                        &pEventListenPortAsync->eventAsync,
                                        inLoop_eventListenPort_postAccept,
                                        inLoop_eventListenPort_cancel);
            }
        }
        return true;
    }
    return false;
}

void eventListenPort_close(eventListenPort_tt* pHandle)
{
    bool bActive = true;
    if (atomic_compare_exchange_strong(&pHandle->bActive, &bActive, false)) {
        if (pHandle->pEventIO->uiCocurrentThreads == 0) {
            if (pHandle->pListenHandle->pEventListenPort != NULL) {
                atomic_fetch_add(&pHandle->iRefCount, 1);
                eventListenPortAsync_tt* pEventListenPortAsync =
                    mem_malloc(sizeof(eventListenPortAsync_tt));
                pEventListenPortAsync->pListenHandle = pHandle->pListenHandle;
                eventIOLoop_queueInLoop(pHandle->pListenHandle->pEventIOLoop,
                                        &pEventListenPortAsync->eventAsync,
                                        inLoop_eventListenPort_close,
                                        inLoop_eventListenPort_close);
            }
        }
        else {
            for (uint32_t i = 0; i < pHandle->pEventIO->uiCocurrentThreads; ++i) {
                if (pHandle->pListenHandle[i].pEventListenPort != NULL) {
                    atomic_fetch_add(&pHandle->iRefCount, 1);
                    eventListenPortAsync_tt* pEventListenPortAsync =
                        mem_malloc(sizeof(eventListenPortAsync_tt));
                    pEventListenPortAsync->pListenHandle = &(pHandle->pListenHandle[i]);
                    eventIOLoop_queueInLoop(pHandle->pListenHandle[i].pEventIOLoop,
                                            &pEventListenPortAsync->eventAsync,
                                            inLoop_eventListenPort_close,
                                            inLoop_eventListenPort_close);
                }
            }
        }
    }
}

bool eventListenPort_postAccept(eventListenPort_tt* pHandle)
{
    if (!atomic_load(&pHandle->bActive)) {
        return false;
    }

    if (pHandle->pEventIO->uiCocurrentThreads == 0) {
        if (pHandle->pListenHandle->pEventListenPort == NULL) {
            atomic_fetch_add(&pHandle->iRefCount, 1);
            pHandle->pListenHandle->pEventListenPort = pHandle;
            eventListenPortAsync_tt* pEventListenPortAsync =
                mem_malloc(sizeof(eventListenPortAsync_tt));
            pEventListenPortAsync->pListenHandle = pHandle->pListenHandle;
            eventIOLoop_queueInLoop(pHandle->pListenHandle->pEventIOLoop,
                                    &pEventListenPortAsync->eventAsync,
                                    inLoop_eventListenPort_postAccept,
                                    inLoop_eventListenPort_cancel);
            return true;
        }
    }
    else {
        for (uint32_t i = 0; i < pHandle->pEventIO->uiCocurrentThreads; ++i) {
            if (pHandle->pListenHandle[i].pEventListenPort == NULL) {
                atomic_fetch_add(&pHandle->iRefCount, 1);
                pHandle->pListenHandle[i].pEventListenPort = pHandle;
                eventListenPortAsync_tt* pEventListenPortAsync =
                    mem_malloc(sizeof(eventListenPortAsync_tt));
                pEventListenPortAsync->pListenHandle = &(pHandle->pListenHandle[i]);
                eventIOLoop_queueInLoop(pHandle->pListenHandle[i].pEventIOLoop,
                                        &pEventListenPortAsync->eventAsync,
                                        inLoop_eventListenPort_postAccept,
                                        inLoop_eventListenPort_cancel);
                return true;
            }
        }
    }
    return false;
}

void setAcceptRecvFromFilterCallback(bool (*fn)(const inetAddress_tt*, const char*, uint32_t))
{
    s_fnRecvFromFilterCallback = fn;
}
