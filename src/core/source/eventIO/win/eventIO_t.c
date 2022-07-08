

#include "eventIO/eventIO_t.h"

#include "time_t.h"
#include "thread_t.h"
#include "queue_t.h"
#include "heap_t.h"
#include "log_t.h"
#include "utility_t.h"

#include "eventIO/internal/win/iocpExt_t.h"

#include "eventIO/internal/win/eventWatcher_t.h"
#include "eventIO/internal/win/eventConnection_t.h"
#include "eventIO/internal/win/eventListenPort_t.h"
#include "eventIO/internal/win/eventIO-inl.h"
#include "eventIO/internal/eventTimer_t.h"

#ifndef WSAID_ACCEPTEX
#    define WSAID_ACCEPTEX                                     \
        {                                                      \
            0xb5367df1, 0xcbac, 0x11cf,                        \
            {                                                  \
                0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92 \
            }                                                  \
        }
#endif
#ifndef WSAID_CONNECTEX
#    define WSAID_CONNECTEX                                    \
        {                                                      \
            0x25a207b9, 0xddf3, 0x4660,                        \
            {                                                  \
                0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e \
            }                                                  \
        }
#endif
#ifndef WSAID_GETACCEPTEXSOCKADDRS
#    define WSAID_GETACCEPTEXSOCKADDRS                         \
        {                                                      \
            0xb5367df2, 0xcbac, 0x11cf,                        \
            {                                                  \
                0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92 \
            }                                                  \
        }
#endif

#ifndef WSAID_DISCONNECTEX
#    define WSAID_DISCONNECTEX                                 \
        {                                                      \
            0x7fda2e11, 0x8630, 0x436f,                        \
            {                                                  \
                0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57 \
            }                                                  \
        }
#endif

static void* GetExtensionFunction(SOCKET s, const GUID* which_fn)
{
    void* ptr   = NULL;
    DWORD bytes = 0;
    WSAIoctl(s,
             SIO_GET_EXTENSION_FUNCTION_POINTER,
             (GUID*)which_fn,
             sizeof(*which_fn),
             &ptr,
             sizeof(ptr),
             &bytes,
             NULL,
             NULL);
    return ptr;
}

static AcceptExPtr                    s_acceptEx                    = NULL;
static ConnectExPtr                   s_connectEx                   = NULL;
static DisconnectExPtr                s_disconnectEx                = NULL;
static GetAcceptExSockaddrsPtr        s_getAcceptExSockaddrs        = NULL;
static GetQueuedCompletionStatusExPtr s_getQueuedCompletionStatusEx = NULL;
static CancelIoExPtr                  s_cancelIoEx                  = NULL;

static bool initExtensionFunctions()
{
    const GUID acceptex             = WSAID_ACCEPTEX;
    const GUID connectex            = WSAID_CONNECTEX;
    const GUID disconnectex         = WSAID_DISCONNECTEX;
    const GUID getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
    SOCKET     s                    = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return false;
    s_acceptEx  = (AcceptExPtr)GetExtensionFunction(s, &acceptex);
    s_connectEx = (ConnectExPtr)GetExtensionFunction(s, &connectex);
    ;
    s_disconnectEx = (DisconnectExPtr)GetExtensionFunction(s, &disconnectex);
    ;
    s_getAcceptExSockaddrs =
        (GetAcceptExSockaddrsPtr)GetExtensionFunction(s, &getacceptexsockaddrs);
    closesocket(s);

    HMODULE kernel32_module = GetModuleHandle(TEXT("kernel32.dll"));
    if (kernel32_module == NULL) {
        return false;
    }
    s_getQueuedCompletionStatusEx = (GetQueuedCompletionStatusExPtr)GetProcAddress(
        kernel32_module, "GetQueuedCompletionStatusEx");

    s_cancelIoEx = (CancelIoExPtr)GetProcAddress(kernel32_module, "CancelIoEx");
    return true;
}

BOOL acceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, PVOID lpOutputBuffer,
              DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
              LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
{
    return s_acceptEx(sListenSocket,
                      sAcceptSocket,
                      lpOutputBuffer,
                      dwReceiveDataLength,
                      dwLocalAddressLength,
                      dwRemoteAddressLength,
                      lpdwBytesReceived,
                      lpOverlapped);
}

void getAcceptExSockaddrs(PVOID lpOutputBuffer, DWORD dwReceiveDataLength,
                          DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength,
                          LPSOCKADDR* LocalSockaddr, LPINT LocalSockaddrLength,
                          LPSOCKADDR* RemoteSockaddr, LPINT RemoteSockaddrLength)
{
    return s_getAcceptExSockaddrs(lpOutputBuffer,
                                  dwReceiveDataLength,
                                  dwLocalAddressLength,
                                  dwRemoteAddressLength,
                                  LocalSockaddr,
                                  LocalSockaddrLength,
                                  RemoteSockaddr,
                                  RemoteSockaddrLength);
}

BOOL connectEx(SOCKET s, const struct sockaddr* name, int namelen, PVOID lpSendBuffer,
               DWORD dwSendDataLength, LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped)
{
    return s_connectEx(
        s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
}

BOOL disconnectEx(SOCKET hSocket, LPOVERLAPPED lpOverlapped, DWORD dwFlags, DWORD reserved)
{
    return s_disconnectEx(hSocket, lpOverlapped, dwFlags, reserved);
}

BOOL getQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
                                 ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds,
                                 BOOL bAlertable)
{
    return s_getQueuedCompletionStatusEx(CompletionPort,
                                         lpCompletionPortEntries,
                                         ulCount,
                                         ulNumEntriesRemoved,
                                         dwMilliseconds,
                                         bAlertable);
}

BOOL cancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
    return s_cancelIoEx(hFile, lpOverlapped);
}

static inline int32_t setSocketNonblocking(SOCKET hSocket)
{
    unsigned long nb = 1;
    return ioctlsocket(hSocket, FIONBIO, &nb);
}

static atomic_int s_iSocketStartup = ATOMIC_VAR_INIT(0);

void socketStartup()
{
    if (atomic_load(&s_iSocketStartup) == 0) {
        WSADATA wsaData;
        if (WSAStartup(0x0202, &wsaData) != 0) {
            DLog(eLog_error, "WSAStartup error");
        }
    }
    atomic_fetch_add(&s_iSocketStartup, 1);
}

void socketCleanup()
{
    if (atomic_fetch_sub(&s_iSocketStartup, 1) == 1) {
        WSACleanup();
    }
}

typedef struct socketReuse_s
{
    void*  node[2];
    SOCKET hSocket;
} socketReuse_tt;

SOCKET eventIO_makeSocket(struct eventIO_s* pEventIO)
{
    SOCKET          hSocket      = INVALID_SOCKET;
    socketReuse_tt* pSocketReuse = NULL;
    QUEUE*          pNode        = NULL;

#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIO->socketReuseLock);
#else
    mutex_lock(&pEventIO->socketReuseLock);
#endif
    if (!QUEUE_EMPTY(&pEventIO->queueSocketReuse)) {
        pNode = QUEUE_HEAD(&pEventIO->queueSocketReuse);
        QUEUE_REMOVE(pNode);
        pSocketReuse = container_of(pNode, socketReuse_tt, node);
    }
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIO->socketReuseLock);
#else
    mutex_unlock(&pEventIO->socketReuseLock);
#endif
    if (pSocketReuse == NULL) {
        hSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (hSocket != INVALID_SOCKET) {
            setSocketNonblocking(hSocket);
            if (CreateIoCompletionPort(
                    (HANDLE)hSocket, pEventIO->hCompletionPort, def_IOCP_CONNECTION, 0) == NULL) {
                closesocket(hSocket);
                hSocket        = INVALID_SOCKET;
                int32_t iError = WSAGetLastError();
                Log(eLog_error,
                    "eventIO_makeTcpSocket CreateIoCompletionPort error, errno=%d",
                    iError);
            }
        }
    }
    else {
        hSocket = pSocketReuse->hSocket;
        mem_free(pSocketReuse);
    }
    return hSocket;
}

void eventIO_recoverySocket(struct eventIO_s* pEventIO, SOCKET hSocket)
{
    socketReuse_tt* pSocketReuse = mem_malloc(sizeof(socketReuse_tt));
    pSocketReuse->hSocket        = hSocket;
#ifdef DEF_USE_SPINLOCK
    spinLock_lock(&pEventIO->socketReuseLock);
#else
    mutex_lock(&pEventIO->socketReuseLock);
#endif
    QUEUE_INSERT_TAIL(&pEventIO->queueSocketReuse, &pSocketReuse->node);
#ifdef DEF_USE_SPINLOCK
    spinLock_unlock(&pEventIO->socketReuseLock);
#else
    mutex_unlock(&pEventIO->socketReuseLock);
#endif
}

static inline size_t Min(const size_t a, const size_t b)
{
    return a <= b ? a : b;
}

eventIO_tt* createEventIO()
{
    socketStartup();
    initExtensionFunctions();
    eventIO_tt* pEventIO         = mem_malloc(sizeof(eventIO_tt));
    pEventIO->uiCocurrentThreads = 0;
    pEventIO->uiLoopTime         = 0;
    pEventIO->uiTimerCounter     = 0;
    pEventIO->bTimerEventOff     = false;
    pEventIO->bRunning           = false;
    pEventIO->bLoopSleep         = false;
    cond_init(&pEventIO->cond);
    pEventIO->hCompletionPort = NULL;
    heap_init((struct heap*)&pEventIO->timerHeap);
    QUEUE_INIT(&pEventIO->queuePending);
    QUEUE_INIT(&pEventIO->queueSocketReuse);

#ifdef DEF_USE_SPINLOCK
    spinLock_init(&pEventIO->socketReuseLock);
#else
    mutex_init(&pEventIO->socketReuseLock);
#endif

    mutex_init(&pEventIO->mutex);
    atomic_init(&pEventIO->iRefCount, 1);
    atomic_init(&pEventIO->uiCocurrentRunning, 0);
    atomic_init(&pEventIO->bLoopNotified, true);
    atomic_init(&pEventIO->bLoopRunning, false);
    return pEventIO;
}

static inline bool eventIO_isInLoopThread(eventIO_tt* pEventIO)
{
    return pEventIO->uiThreadId == threadId();
}

void eventIO_queueInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                         void (*fnWork)(eventAsync_tt*), void (*fnCancel)(eventAsync_tt*))
{
    pEventAsync->fnWork   = fnWork;
    pEventAsync->fnCancel = fnCancel;

    if (pEventIO->uiCocurrentThreads == 0) {
        mutex_lock(&pEventIO->mutex);
        QUEUE_INSERT_TAIL(&pEventIO->queuePending, &pEventAsync->node);
        mutex_unlock(&pEventIO->mutex);

        if (!atomic_load(&pEventIO->bLoopNotified)) {
            atomic_store(&pEventIO->bLoopNotified, true);
            PostQueuedCompletionStatus(pEventIO->hCompletionPort, 0, def_IOCP_TASK, NULL);
        }
    }
    else {
        mutex_lock(&pEventIO->mutex);
        QUEUE_INSERT_TAIL(&pEventIO->queuePending, &pEventAsync->node);
        if (pEventIO->bLoopSleep) {
            cond_signal(&pEventIO->cond);
        }
        mutex_unlock(&pEventIO->mutex);
    }
}

void eventIO_runInLoop(eventIO_tt* pEventIO, eventAsync_tt* pEventAsync,
                       void (*fnWork)(eventAsync_tt*), void (*fnCancel)(eventAsync_tt*))
{
    if (eventIO_isInLoopThread(pEventIO)) {
        if (eventIO_isRunning(pEventIO)) {
            fnWork(pEventAsync);
        }
        else {
            if (fnCancel) {
                fnCancel(pEventAsync);
            }
        }
    }
    else {
        eventIO_queueInLoop(pEventIO, pEventAsync, fnWork, fnCancel);
    }
}

void eventIO_setConcurrentThreads(eventIO_tt* pEventIO, uint32_t uiCocurrentThreads)
{
    if (!atomic_load(&pEventIO->bLoopRunning)) {
        pEventIO->uiCocurrentThreads = uiCocurrentThreads;
    }
}

int32_t eventIO_getIdleThreads(eventIO_tt* pEventIO)
{
    return atomic_load(&pEventIO->iIdleThreads);
}

uint32_t eventIO_getNumberOfConcurrentThreads(eventIO_tt* pEventIO)
{
    return pEventIO->uiCocurrentThreads;
}

void eventIO_addref(eventIO_tt* pEventIO)
{
    atomic_fetch_add(&(pEventIO->iRefCount), 1);
}

void eventIO_release(eventIO_tt* pEventIO)
{
    if (atomic_fetch_sub(&(pEventIO->iRefCount), 1) == 1) {
#ifndef DEF_USE_SPINLOCK
        mutex_destroy(&pEventIO->socketReuseLock);
#endif
        cond_destroy(&pEventIO->cond);
        mutex_destroy(&pEventIO->mutex);
        mem_free(pEventIO);
        socketCleanup();
    }
}

typedef struct eventIOStopAsync_s
{
    eventIO_tt*   pEventIO;
    eventAsync_tt eventAsync;
} eventIOStopAsync_tt;

static void inLoop_eventIO_stop(eventAsync_tt* pEventAsync)
{
    eventIOStopAsync_tt* pEventIOStopAsync =
        container_of(pEventAsync, eventIOStopAsync_tt, eventAsync);
    eventIO_tt* pEventIO = pEventIOStopAsync->pEventIO;
    pEventIO->bRunning   = false;
    eventIO_release(pEventIO);
    mem_free(pEventIOStopAsync);
}

void eventIO_stopLoop(eventIO_tt* pEventIO)
{
    if (atomic_fetch_sub(&(pEventIO->uiCocurrentRunning), 1) == 1) {
        eventIOStopAsync_tt* pEventIOStopAsync = mem_malloc(sizeof(eventIOStopAsync_tt));
        pEventIOStopAsync->pEventIO            = pEventIO;
        atomic_fetch_add(&(pEventIO->iRefCount), 1);
        eventIO_queueInLoop(
            pEventIO, &pEventIOStopAsync->eventAsync, inLoop_eventIO_stop, inLoop_eventIO_stop);
    }
}

static inline int32_t eventIO_nextTimeout(const eventIO_tt* pEventIO)
{
    const struct heap_node* pHeapNode = heap_min(&pEventIO->timerHeap);
    if (pHeapNode == NULL) return -1;

    const eventTimer_tt* pHandle = container_of(pHeapNode, eventTimer_tt, node);
    if (pHandle->uiTimeout <= pEventIO->uiLoopTime) return 0;

    return (int32_t)(pHandle->uiTimeout - pEventIO->uiLoopTime);
}

static inline void eventIO_runTimers(eventIO_tt* pEventIO)
{
    struct heap_node* pHeadNode = NULL;
    eventTimer_tt*    pHandle   = NULL;

    for (;;) {
        pHeadNode = heap_min(&pEventIO->timerHeap);
        if (pHeadNode == NULL) break;

        pHandle = container_of(pHeadNode, eventTimer_tt, node);
        if (pHandle->uiTimeout > pEventIO->uiLoopTime) break;
        eventTimer_run(pHandle);
    }
}

static void eventIO_queuedCompletionStatusEx(eventIO_tt* pEventIO)
{
    ULONG             uiOverlappedEntryNum = 32;
    OVERLAPPED_ENTRY* pOverlappedEntrys =
        (OVERLAPPED_ENTRY*)mem_malloc(uiOverlappedEntryNum * sizeof(OVERLAPPED_ENTRY));
    bool        bLoopRunning = true;
    timespec_tt time;
    ULONG       uiCount       = 0;
    DWORD       dwWaitTimeout = INFINITE;

    while (bLoopRunning) {
        if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
            getClockMonotonic(&time);
            pEventIO->uiLoopTime = timespec_toMsec(&time);
            dwWaitTimeout        = eventIO_nextTimeout(pEventIO);
        }

        bzero(pOverlappedEntrys, uiOverlappedEntryNum * sizeof(OVERLAPPED_ENTRY));
        atomic_fetch_add(&pEventIO->iIdleThreads, 1);
        bool bSuccess = s_getQueuedCompletionStatusEx(pEventIO->hCompletionPort,
                                                      pOverlappedEntrys,
                                                      uiOverlappedEntryNum,
                                                      &uiCount,
                                                      dwWaitTimeout,
                                                      FALSE)
                            ? true
                            : false;
        atomic_fetch_sub(&pEventIO->iIdleThreads, 1);
        if (bSuccess) {
            for (ULONG i = 0; i < uiCount; ++i) {
                switch (pOverlappedEntrys[i].lpCompletionKey) {
                case def_IOCP_QUEUED_QUIT:
                {
                    if (bLoopRunning) {
                        bLoopRunning = false;
                    }
                    else {
                        PostQueuedCompletionStatus(
                            pEventIO->hCompletionPort, 0, def_IOCP_QUEUED_QUIT, NULL);
                    }
                } break;
                case def_IOCP_CONNECTION:
                {
                    if (pOverlappedEntrys[i].lpOverlapped) {
                        eventConnection_overlappedPlus_tt* pOverlappedPlus =
                            CONTAINING_RECORD(pOverlappedEntrys[i].lpOverlapped,
                                              eventConnection_overlappedPlus_tt,
                                              _Overlapped);
                        eventConnection_tt* pEventConnection = pOverlappedPlus->pEventConnection;
                        switch (pOverlappedPlus->eOperation) {
                        case eConnectOp:
                        {
                            eventConnection_onConnect(pEventConnection, pOverlappedPlus);
                        } break;
                        case eDisconnectOp:
                        {
                            eventConnection_onDisconnect(pEventConnection, pOverlappedPlus);
                        } break;
                        case eRecvOp:
                        {
                            eventConnection_onRecv(pEventConnection,
                                                   pOverlappedPlus,
                                                   pOverlappedEntrys[i].dwNumberOfBytesTransferred);
                        } break;
                        case eSendOp:
                        {
                            eventConnection_onSend(pEventConnection,
                                                   pOverlappedPlus,
                                                   pOverlappedEntrys[i].dwNumberOfBytesTransferred);
                        } break;
                        }
                    }
                } break;
                case def_IOCP_ACCEPT:
                {
                    if (pOverlappedEntrys[i].lpOverlapped) {
                        acceptOverlappedPlus_tt* pOverlappedPlus =
                            CONTAINING_RECORD(pOverlappedEntrys[i].lpOverlapped,
                                              acceptOverlappedPlus_tt,
                                              _Overlapped);
                        eventListenPort_tt* pEventListenPort = pOverlappedPlus->pEventListenPort;
                        eventListenPort_onAccept(pEventListenPort,
                                                 pOverlappedPlus,
                                                 pOverlappedEntrys[i].dwNumberOfBytesTransferred);
                    }
                } break;
                case def_IOCP_RECVFROM:
                {
                    if (pOverlappedEntrys[i].lpOverlapped) {
                        recvFromOverlappedPlus_tt* pOverlappedPlus =
                            CONTAINING_RECORD(pOverlappedEntrys[i].lpOverlapped,
                                              recvFromOverlappedPlus_tt,
                                              _Overlapped);
                        eventListenPort_tt* pEventListenPort = pOverlappedPlus->pEventListenPort;
                        eventListenPort_onRecvFrom(pEventListenPort,
                                                   pOverlappedPlus,
                                                   pOverlappedEntrys[i].dwNumberOfBytesTransferred);
                    }
                } break;
                case def_IOCP_EVENT:
                {
                    // atomic_fetch_add(&(pEventIO->iIdleThreads),1);
                    if (pOverlappedEntrys[i].lpOverlapped) {
                        eventWatcher_overlappedPlus_tt* pOverlappedPlus =
                            CONTAINING_RECORD(pOverlappedEntrys[i].lpOverlapped,
                                              eventWatcher_overlappedPlus_tt,
                                              _Overlapped);
                        pOverlappedPlus->fn(pOverlappedPlus->pEventWatcher);
                    }
                } break;
                case def_IOCP_TASK:
                {
                    QUEUE queuePending;
                    atomic_store(&pEventIO->bLoopNotified, false);
                    mutex_lock(&pEventIO->mutex);
                    QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                    mutex_unlock(&pEventIO->mutex);

                    if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
                        getClockMonotonic(&time);
                        pEventIO->uiLoopTime = timespec_toMsec(&time);
                    }

                    eventAsync_tt* pEvent = NULL;
                    QUEUE*         pNode  = NULL;
                    while (!QUEUE_EMPTY(&queuePending)) {
                        pNode = QUEUE_HEAD(&queuePending);
                        QUEUE_REMOVE(pNode);

                        pEvent = container_of(pNode, eventAsync_tt, node);
                        if (atomic_load(&pEventIO->bLoopRunning)) {
                            pEvent->fnWork(pEvent);
                        }
                        else {
                            if (pEvent->fnCancel) {
                                pEvent->fnCancel(pEvent);
                            }
                            else {
                                mem_free(pEvent);
                            }
                        }
                    }
                } break;
                }
            }
        }
        else {
            if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
                pEventIO->uiLoopTime += dwWaitTimeout;
                eventIO_runTimers(pEventIO);
            }
        }

        if (uiCount == uiOverlappedEntryNum && uiOverlappedEntryNum != 256) {
            ULONG uiReallocOverlappedEntryNum = Min(uiOverlappedEntryNum * 2, 256UL);

            OVERLAPPED_ENTRY* pNewOverlappedEntrys = (OVERLAPPED_ENTRY*)mem_realloc(
                pOverlappedEntrys, uiReallocOverlappedEntryNum * sizeof(OVERLAPPED_ENTRY));
            if (pNewOverlappedEntrys) {
                pOverlappedEntrys    = pNewOverlappedEntrys;
                uiOverlappedEntryNum = uiReallocOverlappedEntryNum;
            }
        }
    }

    if (pEventIO->uiCocurrentThreads != 0) {
        eventIO_stopLoop(pEventIO);
    }

    mem_free(pOverlappedEntrys);
}

static void eventIO_queuedCompletionStatus(eventIO_tt* pEventIO)
{
    DWORD        dwTransferred   = 0;
    LPOVERLAPPED lpOverlapped    = NULL;
    ULONG_PTR    uiCompletionKey = 0;
    bool         bLoopRunning    = true;
    timespec_tt  time;

    while (bLoopRunning) {
        DWORD dwWaitTimeout = INFINITE;
        if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
            getClockMonotonic(&time);
            pEventIO->uiLoopTime = timespec_toMsec(&time);
            dwWaitTimeout        = eventIO_nextTimeout(pEventIO);
        }

        atomic_fetch_add(&pEventIO->iIdleThreads, 1);
        bool bSuccess = GetQueuedCompletionStatus(pEventIO->hCompletionPort,
                                                  &dwTransferred,
                                                  &uiCompletionKey,
                                                  &lpOverlapped,
                                                  dwWaitTimeout)
                            ? true
                            : false;
        atomic_fetch_sub(&pEventIO->iIdleThreads, 1);
        if (bSuccess) {
            switch (uiCompletionKey) {
            case def_IOCP_QUEUED_QUIT:
            {
                if (bLoopRunning) {
                    bLoopRunning = false;
                }
                else {
                    PostQueuedCompletionStatus(
                        pEventIO->hCompletionPort, 0, def_IOCP_QUEUED_QUIT, NULL);
                }
            } break;
            case def_IOCP_CONNECTION:
            {
                if (lpOverlapped) {
                    eventConnection_overlappedPlus_tt* pOverlappedPlus = CONTAINING_RECORD(
                        lpOverlapped, eventConnection_overlappedPlus_tt, _Overlapped);
                    eventConnection_tt* pEventConnection = pOverlappedPlus->pEventConnection;
                    switch (pOverlappedPlus->eOperation) {
                    case eConnectOp:
                    {
                        eventConnection_onConnect(pEventConnection, pOverlappedPlus);
                    } break;
                    case eDisconnectOp:
                    {
                        eventConnection_onDisconnect(pEventConnection, pOverlappedPlus);
                    } break;
                    case eRecvOp:
                    {
                        eventConnection_onRecv(pEventConnection, pOverlappedPlus, dwTransferred);
                    } break;
                    case eSendOp:
                    {
                        eventConnection_onSend(pEventConnection, pOverlappedPlus, dwTransferred);
                    } break;
                    }
                }
            } break;
            case def_IOCP_ACCEPT:
            {
                if (lpOverlapped) {
                    acceptOverlappedPlus_tt* pOverlappedPlus =
                        CONTAINING_RECORD(lpOverlapped, acceptOverlappedPlus_tt, _Overlapped);
                    eventListenPort_tt* pEventListenPort = pOverlappedPlus->pEventListenPort;
                    eventListenPort_onAccept(pEventListenPort, pOverlappedPlus, dwTransferred);
                }
            } break;
            case def_IOCP_RECVFROM:
            {
                if (lpOverlapped) {
                    recvFromOverlappedPlus_tt* pOverlappedPlus =
                        CONTAINING_RECORD(lpOverlapped, recvFromOverlappedPlus_tt, _Overlapped);
                    eventListenPort_tt* pEventListenPort = pOverlappedPlus->pEventListenPort;
                    eventListenPort_onRecvFrom(pEventListenPort, pOverlappedPlus, dwTransferred);
                }
            } break;
            case def_IOCP_EVENT:
            {
                // atomic_fetch_add(&(pEventIO->iIdleThreads),1);
                if (lpOverlapped) {
                    eventWatcher_overlappedPlus_tt* pOverlappedPlus = CONTAINING_RECORD(
                        lpOverlapped, eventWatcher_overlappedPlus_tt, _Overlapped);
                    pOverlappedPlus->fn(pOverlappedPlus->pEventWatcher);
                }
            } break;
            case def_IOCP_TASK:
            {
                QUEUE queuePending;
                atomic_store(&pEventIO->bLoopNotified, false);
                mutex_lock(&pEventIO->mutex);
                QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                mutex_unlock(&pEventIO->mutex);

                if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
                    getClockMonotonic(&time);
                    pEventIO->uiLoopTime = timespec_toMsec(&time);
                }

                eventAsync_tt* pEvent = NULL;
                QUEUE*         pNode  = NULL;
                while (!QUEUE_EMPTY(&queuePending)) {
                    pNode = QUEUE_HEAD(&queuePending);
                    QUEUE_REMOVE(pNode);

                    pEvent = container_of(pNode, eventAsync_tt, node);
                    if (atomic_load(&pEventIO->bLoopRunning)) {
                        pEvent->fnWork(pEvent);
                    }
                    else {
                        if (pEvent->fnCancel) {
                            pEvent->fnCancel(pEvent);
                        }
                        else {
                            mem_free(pEvent);
                        }
                    }
                }
            } break;
            }
        }
        else {
            if (pEventIO->uiCocurrentThreads == 0 && !pEventIO->bTimerEventOff) {
                pEventIO->uiLoopTime += dwWaitTimeout;
                eventIO_runTimers(pEventIO);
            }
        }
    }

    if (pEventIO->uiCocurrentThreads != 0) {
        eventIO_stopLoop(pEventIO);
    }
}

void eventIO_dispatch(eventIO_tt* pEventIO)
{
    pEventIO->uiThreadId = threadId();
    QUEUE queuePending;
    QUEUE_INIT(&queuePending);

    if (pEventIO->uiCocurrentThreads == 0) {
        if (s_getQueuedCompletionStatusEx) {
            eventIO_queuedCompletionStatusEx(pEventIO);
        }
        else {
            eventIO_queuedCompletionStatus(pEventIO);
        }
    }
    else {
        timespec_tt    time;
        int32_t        iWaitTimeout = -1;
        eventAsync_tt* pEvent       = NULL;
        QUEUE*         pNode        = NULL;

        getClockMonotonic(&time);
        pEventIO->uiLoopTime = timespec_toMsec(&time);

        while (pEventIO->bRunning) {
            mutex_lock(&pEventIO->mutex);
            if (QUEUE_EMPTY(&pEventIO->queuePending)) {
                pEventIO->bLoopSleep = true;
                if (iWaitTimeout == -1) {
                    cond_wait(&pEventIO->cond, &pEventIO->mutex);
                    pEventIO->bLoopSleep = false;
                    QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                    mutex_unlock(&pEventIO->mutex);
                    getClockMonotonic(&time);
                    pEventIO->uiLoopTime = timespec_toMsec(&time);
                }
                else {
                    int32_t iStatus =
                        cond_timedwait(&pEventIO->cond, &pEventIO->mutex, iWaitTimeout * 1e6);
                    switch (iStatus) {
                    case eThreadSuccess:
                    {
                        pEventIO->bLoopSleep = false;
                        QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                        mutex_unlock(&pEventIO->mutex);
                        getClockMonotonic(&time);
                        pEventIO->uiLoopTime = timespec_toMsec(&time);
                    } break;
                    case eThreadTimedout:
                    {
                        pEventIO->bLoopSleep = false;
                        mutex_unlock(&pEventIO->mutex);
                        pEventIO->uiLoopTime += iWaitTimeout;
                        eventIO_runTimers(pEventIO);
                        getClockMonotonic(&time);
                        pEventIO->uiLoopTime = timespec_toMsec(&time);
                    } break;
                    }
                }
            }
            else {
                pEventIO->bLoopSleep = false;
                QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
                mutex_unlock(&pEventIO->mutex);
                getClockMonotonic(&time);
                pEventIO->uiLoopTime = timespec_toMsec(&time);
            }

            if (!QUEUE_EMPTY(&queuePending)) {
                do {
                    pNode = QUEUE_HEAD(&queuePending);
                    QUEUE_REMOVE(pNode);

                    pEvent = container_of(pNode, eventAsync_tt, node);
                    if (atomic_load(&pEventIO->bLoopRunning)) {
                        pEvent->fnWork(pEvent);
                    }
                    else {
                        if (pEvent->fnCancel) {
                            pEvent->fnCancel(pEvent);
                        }
                        else {
                            mem_free(pEvent);
                        }
                    }
                } while (!QUEUE_EMPTY(&queuePending));
            }

            if (!pEventIO->bTimerEventOff) {
                iWaitTimeout = eventIO_nextTimeout(pEventIO);
                if (iWaitTimeout == 0) {
                    eventIO_runTimers(pEventIO);
                    iWaitTimeout = eventIO_nextTimeout(pEventIO);
                }
            }
        }
    }

    mutex_lock(&pEventIO->mutex);
    QUEUE_MOVE(&pEventIO->queuePending, &queuePending);
    mutex_unlock(&pEventIO->mutex);
    eventAsync_tt* pEvent = NULL;
    QUEUE*         pNode  = NULL;
    while (!QUEUE_EMPTY(&queuePending)) {
        pNode = QUEUE_HEAD(&queuePending);
        QUEUE_REMOVE(pNode);

        pEvent = container_of(pNode, eventAsync_tt, node);
        if (pEvent->fnCancel) {
            pEvent->fnCancel(pEvent);
        }
        else {
            mem_free(pEvent);
        }
    }

    socketReuse_tt* pSocketReuse = NULL;

    if (!QUEUE_EMPTY(&pEventIO->queueSocketReuse)) {
        pNode = QUEUE_HEAD(&pEventIO->queueSocketReuse);
        QUEUE_REMOVE(pNode);
        pSocketReuse = container_of(pNode, socketReuse_tt, node);
        closesocket(pSocketReuse->hSocket);
        mem_free(pSocketReuse);
    }

    CloseHandle(pEventIO->hCompletionPort);
    pEventIO->hCompletionPort = NULL;
    eventIO_release(pEventIO);
}

static void eventIO_threadLoop(void* pArg)
{
    eventIO_tt* pEventIO = (eventIO_tt*)pArg;
    if (s_getQueuedCompletionStatusEx) {
        eventIO_queuedCompletionStatusEx(pEventIO);
    }
    else {
        eventIO_queuedCompletionStatus(pEventIO);
    }
}

bool eventIO_start(eventIO_tt* pEventIO, bool bTimerEventOff)
{
    pEventIO->uiThreadId = threadId();

    pEventIO->bTimerEventOff = bTimerEventOff;
    QUEUE_INIT(&pEventIO->queuePending);
    QUEUE_INIT(&pEventIO->queueSocketReuse);
    atomic_init(&pEventIO->iIdleThreads, 0);
    if (pEventIO->uiCocurrentThreads != 0) {
        atomic_store(&pEventIO->uiCocurrentRunning, pEventIO->uiCocurrentThreads);
        pEventIO->hCompletionPort =
            CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, pEventIO->uiCocurrentThreads);
        if (pEventIO->hCompletionPort == NULL) {
            int32_t iError = WSAGetLastError();
            Log(eLog_error, "CreateIoCompletionPort error=%d", iError);
            return false;
        }

        for (uint32_t i = 0; i < pEventIO->uiCocurrentThreads; ++i) {
            thread_tt thread;
            thread_start(&thread, eventIO_threadLoop, pEventIO);
        }
    }
    else {
        pEventIO->hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (pEventIO->hCompletionPort == NULL) {
            int32_t iError = WSAGetLastError();
            Log(eLog_error, "CreateIoCompletionPort error=%d", iError);
            return false;
        }
    }
    timespec_tt time;
    getClockMonotonic(&time);
    pEventIO->uiLoopTime = timespec_toMsec(&time);
    atomic_store(&pEventIO->bLoopNotified, false);
    atomic_fetch_add(&(pEventIO->iRefCount), 1);
    atomic_store(&pEventIO->bLoopRunning, true);
    pEventIO->bRunning = true;
    return true;
}

void eventIO_stop(eventIO_tt* pEventIO)
{
    bool bLoopRunning = true;
    if (atomic_compare_exchange_strong(&pEventIO->bLoopRunning, &bLoopRunning, false)) {
        if (pEventIO->uiCocurrentThreads != 0) {
            for (uint32_t i = 0; i < pEventIO->uiCocurrentThreads; ++i) {
                PostQueuedCompletionStatus(
                    pEventIO->hCompletionPort, 0, def_IOCP_QUEUED_QUIT, NULL);
            }
        }
        else {
            PostQueuedCompletionStatus(pEventIO->hCompletionPort, 0, def_IOCP_QUEUED_QUIT, NULL);
        }
    }
}
