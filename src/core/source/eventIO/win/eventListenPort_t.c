

#include "eventIO/internal/win/eventListenPort_t.h"

#include <stdatomic.h>
#include <assert.h>

#include "log_t.h"
#include "eventIO/eventIO_t.h"
#include "eventIO/internal/win/iocpExt_t.h"
#include "eventIO/internal/win/eventIO-inl.h"
#include "eventIO/internal/win/eventConnection_t.h"

const int32_t g_iNumAcceptsSurplus = 32;

static bool (*s_fnRecvFromFilterCallback)(const inetAddress_tt*, const char*, uint32_t);

static inline void setReuseAddr(SOCKET hSocket)
{
    int32_t iOptval = 1;
    int32_t rc      = setsockopt(
        hSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)(&iOptval), (socklen_t)(sizeof iOptval));
    if (rc != 0) {
        int32_t iError = WSAGetLastError();
        Log(eLog_error, "setsockopt(SO_REUSEADDR), errno=%d", iError);
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

static inline int32_t setSocketNonblocking(SOCKET hSocket)
{
    unsigned long nb = 1;
    return ioctlsocket(hSocket, FIONBIO, &nb);
}

static inline bool eventlistenPort_acceptOverlapped(eventListenPort_tt*      pHandle,
                                                    acceptOverlappedPlus_tt* pOverlappedPlus)
{
    bzero(&pOverlappedPlus->_Overlapped, sizeof(OVERLAPPED));
    bzero(pOverlappedPlus->szBuffer, 128);

    DWORD dwTranfered = 0;
    BOOL  bResult     = acceptEx(pHandle->hSocket,
                            pOverlappedPlus->hSocket,
                            (void*)pOverlappedPlus->szBuffer,
                            0,
                            (inetAddress_getSocklen(&pHandle->listenAddr) + 16),
                            (inetAddress_getSocklen(&pHandle->listenAddr) + 16),
                            &dwTranfered,
                            &pOverlappedPlus->_Overlapped);

    if (!bResult) {
        int32_t iError = WSAGetLastError();
        if (iError != ERROR_IO_PENDING) {
            return false;
        }
    }
    return true;
}

static inline bool eventlistenPort_recvFromOverlapped(eventListenPort_tt*        pHandle,
                                                      recvFromOverlappedPlus_tt* pOverlappedPlus)
{
    bzero(&pOverlappedPlus->inetAddress, sizeof(inetAddress_tt));
    bzero(&pOverlappedPlus->_Overlapped, sizeof(OVERLAPPED));
    bzero(pOverlappedPlus->szBuffer, MAXIMUM_MTU_SIZE);

    u_long uiFlag             = 0;
    pOverlappedPlus->iAddrLen = sizeof(inetAddress_tt);

    WSABUF _BufferIO;
    _BufferIO.buf = pOverlappedPlus->szBuffer;
    _BufferIO.len = MAXIMUM_MTU_SIZE;

    int32_t wsaResult = WSARecvFrom(pHandle->hSocket,
                                    &_BufferIO,
                                    1,
                                    NULL,
                                    &uiFlag,
                                    inetAddress_getSockaddr(&pOverlappedPlus->inetAddress),
                                    &pOverlappedPlus->iAddrLen,
                                    &pOverlappedPlus->_Overlapped,
                                    NULL);
    if (wsaResult == SOCKET_ERROR) {
        int32_t iError = WSAGetLastError();
        if (iError != WSA_IO_PENDING) {
            return false;
        }
    }
    return true;
}

void eventListenPort_onAccept(struct eventListenPort_s* pHandle,
                              acceptOverlappedPlus_tt* pOverlappedPlus, uint32_t uiTransferred)
{
    if (atomic_load(&pHandle->bActive)) {
        setsockopt(pOverlappedPlus->hSocket,
                   SOL_SOCKET,
                   SO_UPDATE_ACCEPT_CONTEXT,
                   (char*)&(pHandle->hSocket),
                   sizeof(SOCKET));

        struct sockaddr* pRemote_addr;
        struct sockaddr* pLocal_addr;
        // int32_t iLocal_len = sizeof(struct sockaddr_in);
        // int32_t iRemote_len = sizeof(struct sockaddr_in);
        int32_t iLocal_len  = 0;
        int32_t iRemote_len = 0;
        getAcceptExSockaddrs(pOverlappedPlus->szBuffer,
                             0,
                             (inetAddress_getSocklen(&pHandle->listenAddr) + 16),   // 本地地址长度
                             (inetAddress_getSocklen(&pHandle->listenAddr) + 16),
                             (LPSOCKADDR*)&pLocal_addr,
                             &iLocal_len,
                             (LPSOCKADDR*)&pRemote_addr,
                             &iRemote_len);

        inetAddress_tt localAddr;
        inetAddress_tt remoteAddr;

        if (pLocal_addr->sa_family == AF_INET) {
            inetAddress_init_V4(&localAddr, *((struct sockaddr_in*)pLocal_addr));
        }
        else {
            inetAddress_init_V6(&localAddr, *((struct sockaddr_in6*)pLocal_addr));
        }

        if (pRemote_addr->sa_family == AF_INET) {
            inetAddress_init_V4(&remoteAddr, *((struct sockaddr_in*)pRemote_addr));
        }
        else {
            inetAddress_init_V6(&remoteAddr, *((struct sockaddr_in6*)pRemote_addr));
        }

        eventConnection_tt* pEventConnection = acceptEventConnection(
            pHandle->pEventIO, pHandle, pOverlappedPlus->hSocket, &remoteAddr, &localAddr);
        if (pHandle->fnAcceptCallback) {
            pHandle->fnAcceptCallback(pHandle, pEventConnection, NULL, 0, pHandle->pUserData);
        }

        pOverlappedPlus->hSocket = eventIO_makeSocket(pHandle->pEventIO);
        if (pOverlappedPlus->hSocket == INVALID_SOCKET) {
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            mem_free(pOverlappedPlus);
            eventListenPort_release(pHandle);
            return;
        }

        if (!eventlistenPort_acceptOverlapped(pHandle, pOverlappedPlus)) {
            if (pOverlappedPlus->hSocket != INVALID_SOCKET) {
                closesocket(pOverlappedPlus->hSocket);
                pOverlappedPlus->hSocket = INVALID_SOCKET;
            }
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            mem_free(pOverlappedPlus);
            eventListenPort_release(pHandle);
        }
    }
    else {
        mem_free(pOverlappedPlus);
        eventListenPort_release(pHandle);
    }
}

static void tls_addressConnection_cleanup_func(void* pArg)
{
    mem_free(pArg);
}

void eventListenPort_onRecvFrom(struct eventListenPort_s*  pHandle,
                                recvFromOverlappedPlus_tt* pOverlappedPlus, uint32_t uiTransferred)
{
    if (atomic_load(&pHandle->bActive)) {
        if (uiTransferred == 0) {
            if (!eventlistenPort_recvFromOverlapped(pHandle, pOverlappedPlus)) {
                if (pHandle->fnAcceptCallback) {
                    pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                }
                mem_free(pOverlappedPlus);
                eventListenPort_release(pHandle);
            }
            return;
        }

        static _decl_threadLocal addressConnection_tt* s_pFindAddressConnection = NULL;
        if (s_pFindAddressConnection == NULL) {
            s_pFindAddressConnection = mem_malloc(sizeof(addressConnection_tt));
            setTlsValue(
                pHandle, tls_addressConnection_cleanup_func, s_pFindAddressConnection, true);
        }

        s_pFindAddressConnection->inetAddress = pOverlappedPlus->inetAddress;
        s_pFindAddressConnection->hash        = inetAddress_hash(&pOverlappedPlus->inetAddress);
        s_pFindAddressConnection->pConnection = NULL;

        eventConnection_tt* pEventConnection = NULL;

        rwSpinLock_rdlock(&pHandle->rwlock);
        addressConnection_tt* pNode = RB_FIND(
            addressConnectionMap_s, &pHandle->mapAddressConnection, s_pFindAddressConnection);
        if (pNode != NULL) {
            pEventConnection = pNode->pConnection;
            if (pEventConnection) {
                eventConnection_addref(pEventConnection);
            }
        }
        rwSpinLock_rdunlock(&pHandle->rwlock);

        if (pEventConnection == NULL) {
            if (s_fnRecvFromFilterCallback) {
                if (!s_fnRecvFromFilterCallback(
                        &pOverlappedPlus->inetAddress, pOverlappedPlus->szBuffer, uiTransferred)) {
                    if (!eventlistenPort_recvFromOverlapped(pHandle, pOverlappedPlus)) {
                        if (pHandle->fnAcceptCallback) {
                            pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                        }
                        mem_free(pOverlappedPlus);
                        eventListenPort_release(pHandle);
                    }
                    return;
                }
            }

            rwSpinLock_wrlock(&pHandle->rwlock);
            pNode = RB_INSERT(
                addressConnectionMap_s, &pHandle->mapAddressConnection, s_pFindAddressConnection);
            rwSpinLock_wrunlock(&pHandle->rwlock);
            if (pNode == NULL) {
                SOCKET hSocket =
                    WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
                if (hSocket == INVALID_SOCKET) {
                    rwSpinLock_wrlock(&pHandle->rwlock);
                    RB_REMOVE(addressConnectionMap_s,
                              &pHandle->mapAddressConnection,
                              s_pFindAddressConnection);
                    rwSpinLock_wrunlock(&pHandle->rwlock);

                    if (!eventlistenPort_recvFromOverlapped(pHandle, pOverlappedPlus)) {
                        if (pHandle->fnAcceptCallback) {
                            pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                        }
                        mem_free(pOverlappedPlus);
                        eventListenPort_release(pHandle);
                    }
                    return;
                }

                setSocketNonblocking(hSocket);

                if (CreateIoCompletionPort((HANDLE)hSocket,
                                           pHandle->pEventIO->hCompletionPort,
                                           def_IOCP_CONNECTION,
                                           0) == NULL) {
                    int32_t iError = WSAGetLastError();
                    Log(eLog_error,
                        "eventIO_makeUdpSocket CreateIoCompletionPort error, errno=%d",
                        iError);

                    closesocket(hSocket);

                    rwSpinLock_wrlock(&pHandle->rwlock);
                    RB_REMOVE(addressConnectionMap_s,
                              &pHandle->mapAddressConnection,
                              s_pFindAddressConnection);
                    rwSpinLock_wrunlock(&pHandle->rwlock);

                    if (!eventlistenPort_recvFromOverlapped(pHandle, pOverlappedPlus)) {
                        if (pHandle->fnAcceptCallback) {
                            pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                        }
                        mem_free(pOverlappedPlus);
                        eventListenPort_release(pHandle);
                    }
                    return;
                }

                setDisableUdpConnReset(hSocket);
                setReuseAddr(hSocket);

                if (bind(hSocket,
                         inetAddress_getSockaddr(&pHandle->listenAddr),
                         inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
                    int32_t iError = WSAGetLastError();
                    if (iError != WSAEINVAL) {
                        rwSpinLock_wrlock(&pHandle->rwlock);
                        RB_REMOVE(addressConnectionMap_s,
                                  &pHandle->mapAddressConnection,
                                  s_pFindAddressConnection);
                        rwSpinLock_wrunlock(&pHandle->rwlock);

                        closesocket(hSocket);
                        if (pHandle->fnAcceptCallback) {
                            pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                        }
                        mem_free(pOverlappedPlus);
                        eventListenPort_release(pHandle);
                        return;
                    }
                }

                int32_t iConnectResult =
                    WSAConnect(hSocket,
                               inetAddress_getSockaddr(&pOverlappedPlus->inetAddress),
                               inetAddress_getSocklen(&pOverlappedPlus->inetAddress),
                               NULL,
                               NULL,
                               NULL,
                               NULL);
                if (iConnectResult == SOCKET_ERROR) {
                    int32_t iError = WSAGetLastError();
                    if (iError != WSAEWOULDBLOCK) {
                        rwSpinLock_wrlock(&pHandle->rwlock);
                        RB_REMOVE(addressConnectionMap_s,
                                  &pHandle->mapAddressConnection,
                                  s_pFindAddressConnection);
                        rwSpinLock_wrunlock(&pHandle->rwlock);

                        closesocket(hSocket);
                        if (pHandle->fnAcceptCallback) {
                            pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
                        }
                        mem_free(pOverlappedPlus);
                        eventListenPort_release(pHandle);
                        return;
                    }
                }

                pEventConnection                      = acceptUdpEventConnection(pHandle->pEventIO,
                                                            pHandle,
                                                            hSocket,
                                                            &pOverlappedPlus->inetAddress,
                                                            &pHandle->listenAddr);
                s_pFindAddressConnection->pConnection = pEventConnection;
                s_pFindAddressConnection              = NULL;

                if (pHandle->fnAcceptCallback) {
                    pHandle->fnAcceptCallback(pHandle,
                                              pEventConnection,
                                              pOverlappedPlus->szBuffer,
                                              uiTransferred,
                                              pHandle->pUserData);
                }

                s_pFindAddressConnection = mem_malloc(sizeof(addressConnection_tt));
                setTlsValue(
                    pHandle, tls_addressConnection_cleanup_func, s_pFindAddressConnection, false);
            }
        }
        else {
            eventConnection_receive(pEventConnection, pOverlappedPlus->szBuffer, uiTransferred);
            eventConnection_release(pEventConnection);
        }

        if (!eventlistenPort_recvFromOverlapped(pHandle, pOverlappedPlus)) {
            if (pHandle->fnAcceptCallback) {
                pHandle->fnAcceptCallback(pHandle, NULL, NULL, 0, pHandle->pUserData);
            }
            mem_free(pOverlappedPlus);
            eventListenPort_release(pHandle);
        }
    }
    else {
        mem_free(pOverlappedPlus);
        eventListenPort_release(pHandle);
    }
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
    pHandle->listenAddr         = *pInetAddress;
    pHandle->pUserData          = NULL;
    pHandle->fnUserFree         = NULL;
    pHandle->fnAcceptCallback   = NULL;
    pHandle->hSocket            = INVALID_SOCKET;

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
        mem_free(pHandle);
    }
}

bool eventListenPort_postAccept(eventListenPort_tt* pHandle)
{
    if (!atomic_load(&pHandle->bActive)) {
        return false;
    }
    atomic_fetch_add(&(pHandle->iRefCount), 1);

    if (pHandle->bTcp) {
        SOCKET hSocket = eventIO_makeSocket(pHandle->pEventIO);
        if (hSocket == INVALID_SOCKET) {
            eventListenPort_release(pHandle);
            return false;
        }

        acceptOverlappedPlus_tt* pAcceptOverlapped = mem_malloc(sizeof(acceptOverlappedPlus_tt));
        pAcceptOverlapped->hSocket                 = hSocket;
        pAcceptOverlapped->pEventListenPort        = pHandle;

        if (!eventlistenPort_acceptOverlapped(pHandle, pAcceptOverlapped)) {
            int32_t iError = WSAGetLastError();
            Log(eLog_error, "eventlistenPort_acceptOverlapped, errno=%d", iError);

            if (pAcceptOverlapped->hSocket != INVALID_SOCKET) {
                closesocket(pAcceptOverlapped->hSocket);
                pAcceptOverlapped->hSocket = INVALID_SOCKET;
            }
            mem_free(pAcceptOverlapped);
            eventListenPort_release(pHandle);
            return false;
        }
    }
    else {
        recvFromOverlappedPlus_tt* pRecvFromOverlapped =
            mem_malloc(sizeof(recvFromOverlappedPlus_tt));
        pRecvFromOverlapped->pEventListenPort = pHandle;
        if (!eventlistenPort_recvFromOverlapped(pHandle, pRecvFromOverlapped)) {
            int32_t iError = WSAGetLastError();
            Log(eLog_error, "eventlistenPort_recvFromOverlapped, errno=%d", iError);
            mem_free(pRecvFromOverlapped);
            eventListenPort_release(pHandle);
            return false;
        }
    }
    return true;
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
            pHandle->hSocket =
                WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (pHandle->hSocket == INVALID_SOCKET) {
                atomic_store(&pHandle->bActive, false);
                return false;
            }
        }
        else {
            pHandle->hSocket =
                WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (pHandle->hSocket == INVALID_SOCKET) {
                atomic_store(&pHandle->bActive, false);
                return false;
            }
            setDisableUdpConnReset(pHandle->hSocket);
            setReuseAddr(pHandle->hSocket);
        }

        if (bind(pHandle->hSocket,
                 inetAddress_getSockaddr(&pHandle->listenAddr),
                 inetAddress_getSocklen(&pHandle->listenAddr)) < 0) {
            closesocket(pHandle->hSocket);
            pHandle->hSocket = INVALID_SOCKET;

            atomic_store(&pHandle->bActive, false);
            return false;
        }

        if (pHandle->bTcp) {
            if (CreateIoCompletionPort((HANDLE)pHandle->hSocket,
                                       pHandle->pEventIO->hCompletionPort,
                                       def_IOCP_ACCEPT,
                                       0) == NULL) {
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
                atomic_store(&pHandle->bActive, false);
                return false;
            }
            listen(pHandle->hSocket, SOMAXCONN);
        }
        else {
            if (CreateIoCompletionPort((HANDLE)pHandle->hSocket,
                                       pHandle->pEventIO->hCompletionPort,
                                       def_IOCP_RECVFROM,
                                       0) == NULL) {
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
                atomic_store(&pHandle->bActive, false);
                return false;
            }
        }

        for (int32_t i = 0; i < pHandle->pEventIO->uiCocurrentThreads + g_iNumAcceptsSurplus; ++i) {
            if (!eventListenPort_postAccept(pHandle)) {
                closesocket(pHandle->hSocket);
                pHandle->hSocket = INVALID_SOCKET;
                atomic_store(&pHandle->bActive, false);
                return false;
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
        if (pHandle->hSocket != INVALID_SOCKET) {
            closesocket(pHandle->hSocket);
            pHandle->hSocket = INVALID_SOCKET;
        }
    }
}

void setAcceptRecvFromFilterCallback(bool (*fn)(const inetAddress_tt*, const char*, uint32_t))
{
    s_fnRecvFromFilterCallback = fn;
}
