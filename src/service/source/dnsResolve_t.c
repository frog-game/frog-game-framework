
#define PRINT_MACRO_HELPER(x) #x
#define PRINT_MACRO(x) #x "=" PRINT_MACRO_HELPER(x)

#include "internal/dnsResolve_t.h"

#include <assert.h>

#if DEF_PLATFORM == DEF_PLATFORM_MAC || DEF_PLATFORM == DEF_PLATFORM_MACOS || \
    DEF_PLATFORM == DEF_PLATFORM_IOS || DEF_PLATFORM == DEF_PLATFORM_LINUX || \
    DEF_PLATFORM == DEF_PLATFORM_ANDROID
#    include <netdb.h>
#elif DEF_PLATFORM == DEF_PLATFORM_WINDOWS

struct iovec
{
    void*  iov_base; /* Pointer to data. */
    size_t iov_len;  /* Length of data.  */
};
#endif

#include "internal/service-inl.h"
#include "log_t.h"
#include "msgpack/msgpackEncode_t.h"
#include "serviceEvent_t.h"
#include "service_t.h"
#include "time_t.h"
#include "utility_t.h"

static inline void eventTimer_onClose(eventTimer_tt* pHandle, void* pData)
{
    dnsResolve_tt* pDnsResolve = (dnsResolve_tt*)pData;
    dnsResolve_release(pDnsResolve);
}

static inline void eventTimer_onTimeout(eventTimer_tt* pHandle, void* pData)
{
    dnsResolve_tt* pDnsResolve = (dnsResolve_tt*)pData;

    eventTimer_tt* pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pDnsResolve->hTimeout, 0);
    if (pTimeoutHandle) {
        eventTimer_release(pTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pDnsResolve->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);

        serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt));
        pEvent->uiSourceID      = service_getID(pDnsResolve->pService);
        pEvent->uiToken         = pDnsResolve->uiToken;
        pEvent->uiLength        = DEF_EVENT_DNS << 24;
        service_enqueue(pDnsResolve->pService, pEvent);
    }
}

static inline void eventConnection_onUserFree(void* pData)
{
    dnsResolve_tt* pDnsResolve = (dnsResolve_tt*)pData;
    dnsResolve_release(pDnsResolve);
}

static inline bool eventConnection_onReceiveCallback(eventConnection_tt* pEventConnection,
                                                     byteQueue_tt* pReadByteQueue, void* pData)
{
    dnsResolve_tt* pDnsResolve = (dnsResolve_tt*)pData;

    eventTimer_tt* pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pDnsResolve->hTimeout, 0);
    if (pTimeoutHandle) {
        eventTimer_stop(pTimeoutHandle);
        eventTimer_release(pTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pDnsResolve->hConnection, 0);
    if (pConnectionHandle) {
        size_t           nBytesWritten = byteQueue_getBytesReadable(pReadByteQueue);
        serviceEvent_tt* pEvent        = mem_malloc(sizeof(serviceEvent_tt) + nBytesWritten);
        pEvent->uiSourceID             = service_getID(pDnsResolve->pService);
        pEvent->uiToken                = pDnsResolve->uiToken;
        byteQueue_readBytes(pReadByteQueue, pEvent->szStorage, nBytesWritten, false);
        pEvent->uiLength = nBytesWritten | (DEF_EVENT_DNS << 24);
        service_enqueue(pDnsResolve->pService, pEvent);
    }
    return false;
}

static inline void eventConnection_onDisconnectCallback(eventConnection_tt* pEventConnection,
                                                        void*               pData)
{
    dnsResolve_tt* pDnsResolve    = (dnsResolve_tt*)pData;
    eventTimer_tt* pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pDnsResolve->hTimeout, 0);
    if (pTimeoutHandle) {
        eventTimer_stop(pTimeoutHandle);
        eventTimer_release(pTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pDnsResolve->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);
    }
}

static inline void eventConnection_connectorCallback(eventConnection_tt* pHandle, void* pData)
{
    eventConnection_addref(pHandle);
    dnsResolve_tt* pDnsResolve = (dnsResolve_tt*)pData;
    if (!eventConnection_isConnecting(pHandle)) {
        eventConnection_tt* pConnectionHandle =
            (eventConnection_tt*)atomic_exchange(&pDnsResolve->hConnection, 0);
        if (pConnectionHandle) {
            eventConnection_forceClose(pConnectionHandle);
            eventConnection_release(pConnectionHandle);
        }
    }
    else {
        eventIO_tt* pEventIO = service_getEventIO(pDnsResolve->pService);

        atomic_fetch_add(&pDnsResolve->iRefCount, 1);
        eventTimer_tt* pTimeoutHandle = createEventTimer(
            pEventIO, eventTimer_onTimeout, true, pDnsResolve->uiTimeoutMs, pDnsResolve);
        eventTimer_setCloseCallback(pTimeoutHandle, eventTimer_onClose);
        atomic_store(&pDnsResolve->hTimeout, pTimeoutHandle);
        if (!eventTimer_start(pTimeoutHandle)) {
            pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pDnsResolve->hTimeout, 0);
            if (pTimeoutHandle) {
                eventTimer_stop(pTimeoutHandle);
                eventTimer_release(pTimeoutHandle);
            }
            eventConnection_release(pHandle);
            return;
        }

        atomic_fetch_add(&pDnsResolve->iRefCount, 1);

        eventConnection_setReceiveCallback(pHandle, eventConnection_onReceiveCallback);
        eventConnection_setDisconnectCallback(pHandle, eventConnection_onDisconnectCallback);

        if (!eventConnection_bind(pHandle, false, false, pDnsResolve, eventConnection_onUserFree)) {
            eventConnection_tt* pConnectionHandle =
                (eventConnection_tt*)atomic_exchange(&pDnsResolve->hConnection, 0);
            if (pConnectionHandle) {
                eventConnection_release(pConnectionHandle);
            }
            pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pDnsResolve->hTimeout, 0);
            if (pTimeoutHandle) {
                eventTimer_stop(pTimeoutHandle);
                eventTimer_release(pTimeoutHandle);
            }
            dnsResolve_release(pDnsResolve);
            eventConnection_release(pHandle);
            return;
        }
        int32_t iBufVCount      = pDnsResolve->iBufVCount;
        pDnsResolve->iBufVCount = 0;
        eventConnection_send(pHandle,
                             createEventBuf_move(pDnsResolve->sendBufV, iBufVCount, NULL, 0));
        dnsResolve_release(pDnsResolve);
    }
    eventConnection_release(pHandle);
}

static inline ares_socket_t dns_socket(int32_t iAf, int32_t iType, int32_t iProtocol, void* pData)
{
    dnsResolve_tt*      pHandle = (dnsResolve_tt*)pData;
    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);
    }

    eventTimer_tt* pTimeoutHandle = (eventTimer_tt*)atomic_exchange(&pHandle->hTimeout, 0);
    if (pTimeoutHandle) {
        eventTimer_stop(pTimeoutHandle);
        eventTimer_release(pTimeoutHandle);
    }

    return 1;
}

static inline int32_t dns_close(ares_socket_t s, void* pData)
{
    dnsResolve_tt*      pHandle = (dnsResolve_tt*)pData;
    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);
    }
    return 0;
}

static inline int32_t dns_connect(ares_socket_t sockfd, const struct sockaddr* addr,
                                  ares_socklen_t addrlen, void* pData)
{
    dnsResolve_tt* pHandle = (dnsResolve_tt*)pData;

    if (addr->sa_family == AF_INET) {
        inetAddress_init_V4(&pHandle->recvAddress, *((struct sockaddr_in*)addr));
    }
    else {
        inetAddress_init_V6(&pHandle->recvAddress, *((struct sockaddr_in6*)addr));
    }
    return 1;
}

static inline ares_ssize_t dns_recvfrom(ares_socket_t s, void* pBuffer, size_t nLength,
                                        int32_t iFlags, struct sockaddr* pFrom,
                                        ares_socklen_t* pFrom_len, void* pData)
{
    dnsResolve_tt* pHandle = (dnsResolve_tt*)pData;

    *pFrom_len = inetAddress_getSocklen(&pHandle->recvAddress);
    memcpy(pFrom, inetAddress_getSockaddr(&pHandle->recvAddress), *pFrom_len);

    size_t nBytesWritten = pHandle->nRecvLength - pHandle->nRecvOffset;
    size_t nRead         = nLength > nBytesWritten ? nBytesWritten : nLength;
    if (nRead > 0) {
        memcpy(pBuffer, pHandle->pRecvBuffer + pHandle->nRecvOffset, nRead);
        pHandle->nRecvOffset += nRead;
    }
    return nRead;
}

static ares_ssize_t dns_sendv(ares_socket_t s, const struct iovec* pIoVec, int32_t iCount,
                              void* pData)
{
    dnsResolve_tt* pHandle = (dnsResolve_tt*)pData;

    assert(iCount < 8);

    ares_ssize_t nSendLength = 0;
    pHandle->iBufVCount      = iCount;

    for (int32_t i = 0; i < iCount; ++i) {
        pHandle->sendBufV[i].pBuf = mem_malloc(pIoVec[i].iov_len);
        memcpy(pHandle->sendBufV[i].pBuf, pIoVec[i].iov_base, pIoVec[i].iov_len);
        pHandle->sendBufV[i].iLength = pIoVec[i].iov_len;
        nSendLength += pIoVec[i].iov_len;
    }

    eventIO_tt* pEventIO = service_getEventIO(pHandle->pService);

    atomic_fetch_add(&pHandle->iRefCount, 1);
    eventConnection_tt* pConnectionHandle =
        createEventConnection(pEventIO, &pHandle->recvAddress, false);
    eventConnection_setConnectorCallback(pConnectionHandle, eventConnection_connectorCallback);
    atomic_store(&pHandle->hConnection, pConnectionHandle);
    if (!eventConnection_connect(pConnectionHandle, pHandle, NULL)) {
        pConnectionHandle = (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
        if (pConnectionHandle) {
            eventConnection_release(pConnectionHandle);
            dnsResolve_release(pHandle);
        }
        return -1;
    }
    return nSendLength;
}

static void dns_host_callback(void* pData, int32_t iStatus, int32_t iTimeouts,
                              struct hostent* hostent)
{
    dnsResolve_tt* pHandle = (dnsResolve_tt*)pData;
    pHandle->iAddressCount = 0;
    if (iStatus == ARES_SUCCESS && hostent) {
        for (char** haddr = hostent->h_addr_list; *haddr != NULL; ++haddr) {
            ++pHandle->iAddressCount;
        }
        if (pHandle->iAddressCount > 0) {
            pHandle->szAddressList = mem_malloc(46 * pHandle->iAddressCount);
            bzero(pHandle->szAddressList, 46 * pHandle->iAddressCount);
            char* szAddr = pHandle->szAddressList;
            for (char** haddr = hostent->h_addr_list; *haddr != NULL; ++haddr) {
                inet_ntop(hostent->h_addrtype, *haddr, szAddr, 46);
                szAddr += 46;
            }
        }
    }
}

static atomic_int s_idnsStartup = ATOMIC_VAR_INIT(0);

void dnsStartup()
{
    if (atomic_load(&s_idnsStartup) == 0) {
        if (ares_library_init_mem(ARES_LIB_INIT_ALL, mem_malloc, mem_free, mem_realloc) !=
            ARES_SUCCESS) {
            return;
        }
    }
    atomic_fetch_add(&s_idnsStartup, 1);
}

void dnsCleanup()
{
    if (atomic_fetch_sub(&s_idnsStartup, 1) == 1) {
        ares_library_cleanup();
    }
}

static struct ares_socket_functions socket_functions = {
    dns_socket, dns_close, dns_connect, dns_recvfrom, dns_sendv};

dnsResolve_tt* createDnsResolve(service_tt* pService, bool bLookHostsFile)
{
    dnsResolve_tt*      pHandle = mem_malloc(sizeof(dnsResolve_tt));
    struct ares_options options;
    int32_t             optmask = ARES_OPT_FLAGS | ARES_OPT_TRIES;
    options.flags               = ARES_FLAG_PRIMARY;
    options.flags |= ARES_FLAG_STAYOPEN;
    options.flags |= ARES_FLAG_IGNTC;
    options.tries  = 1;
    char lookups[] = "b";
    if (!bLookHostsFile) {
        optmask |= ARES_OPT_LOOKUPS;
        options.lookups = lookups;
    }

    if (ares_init_options(&pHandle->aresCtx, &options, optmask) != ARES_SUCCESS) {
        Log(eLog_error, "ares_init_options");
        return NULL;
    }

    ares_set_socket_functions(pHandle->aresCtx, &socket_functions, pHandle);

    pHandle->pService = pService;
    service_addref(pHandle->pService);
    pHandle->szAddressList = NULL;
    pHandle->iAddressCount = 0;
    pHandle->uiToken       = 0;
    pHandle->uiTimeoutMs   = 0;
    pHandle->pRecvBuffer   = NULL;
    pHandle->nRecvLength   = 0;
    pHandle->nRecvOffset   = 0;
    pHandle->iBufVCount    = 0;
    bzero(pHandle->sendBufV, sizeof(ioBufVec_tt) * 8);
    atomic_init(&pHandle->hConnection, 0);
    atomic_init(&pHandle->hTimeout, 0);
    atomic_init(&pHandle->iRefCount, 1);
    atomic_init(&pHandle->bActive, false);
    return pHandle;
}

void dnsResolve_addref(dnsResolve_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void dnsResolve_release(dnsResolve_tt* pHandle)
{
    if (atomic_fetch_sub(&pHandle->iRefCount, 1) == 1) {
        if (pHandle->szAddressList) {
            mem_free(pHandle->szAddressList);
            pHandle->szAddressList = NULL;
        }

        for (int32_t i = 0; i < pHandle->iBufVCount; ++i) {
            mem_free(pHandle->sendBufV[i].pBuf);
            pHandle->sendBufV[i].pBuf = NULL;
        }
        pHandle->iBufVCount = 0;

        if (pHandle->pService) {
            service_release(pHandle->pService);
            pHandle->pService = NULL;
            ares_destroy(pHandle->aresCtx);
        }
        mem_free(pHandle);
    }
}

bool dnsResolve_query(dnsResolve_tt* pHandle, const char* szHostName, bool bIPv6,
                      uint32_t uiTimeoutMs, uint32_t uiToken)
{
    bool bActive = false;
    if (atomic_compare_exchange_strong(&pHandle->bActive, &bActive, true)) {
        pHandle->uiToken = uiToken;
        if (pHandle->szAddressList) {
            mem_free(pHandle->szAddressList);
            pHandle->szAddressList = NULL;
        }
        pHandle->iAddressCount = 0;
        pHandle->uiTimeoutMs   = uiTimeoutMs < 5000 ? 5000 : uiTimeoutMs;

        pHandle->iBufVCount = 0;
        bzero(pHandle->sendBufV, sizeof(ioBufVec_tt) * 8);

        int32_t family = AF_INET;
        if (bIPv6) {
            family = AF_INET6;
        }
        ares_gethostbyname(pHandle->aresCtx, szHostName, family, dns_host_callback, pHandle);
        return true;
    }
    return false;
}

int32_t dnsResolve_parser(dnsResolve_tt* pHandle, const char* pBuffer, size_t nLength)
{
    bool bActive = true;
    if (atomic_compare_exchange_strong(&pHandle->bActive, &bActive, false)) {
        if (nLength == 0) {
            ares_process_fd(pHandle->aresCtx, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
        }
        else {
            pHandle->pRecvBuffer = pBuffer;
            pHandle->nRecvLength = nLength;
            pHandle->nRecvOffset = 0;
            ares_process_fd(pHandle->aresCtx, 1, ARES_SOCKET_BAD);
            pHandle->pRecvBuffer = NULL;
            pHandle->nRecvLength = 0;
            pHandle->nRecvOffset = 0;
            return pHandle->iAddressCount;
        }
    }
    return 0;
}

const char* dnsResolve_getAddress(dnsResolve_tt* pHandle, int32_t iIndex)
{
    assert(iIndex < pHandle->iAddressCount);
    return pHandle->szAddressList + iIndex * 46;
}

void dnsResolve_close(dnsResolve_tt* pHandle)
{
    bool bActive = true;
    if (atomic_compare_exchange_strong(&pHandle->bActive, &bActive, false)) {
        ares_process_fd(pHandle->aresCtx, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
    }
}

service_tt* dnsResolve_getService(dnsResolve_tt* pHandle)
{
    return pHandle->pService;
}
