

#include "internal/posix/poller/poller_t.h"
#include "log_t.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define GETERROR errno
#define WSAEWOULDBLOCK EWOULDBLOCK
#define CLOSESOCKET(s) close(s)
#define IOCTLSOCKET(s, c, a) ioctl(s, c, a)
#define CONN_INPRROGRESS EINPROGRESS
#define SHUTDOWN_WR SHUT_WR

#ifndef MSG_NOSIGNAL
#    define MSG_NOSIGNAL 0
#endif

#if defined(__ANDROID__)
typedef unsigned long fd_mask;
#endif

#ifndef fdMaskBits
#    define fdMaskBits (sizeof(fd_mask) * 8)
#endif

#define divRoundUp(x, y) (((x) + ((y)-1)) / (y))

#define selectAllocSize(n) (divRoundUp(n, fdMaskBits) * sizeof(fd_mask))

//#define fdAllocSize(n) ((sizeof(fd_set) + ((n)-1)*sizeof(SOCKET)))

void random_seed(uint32_t* pSeed)
{
    if (*pSeed == 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        *pSeed = (uint32_t)tv.tv_sec + (uint32_t)tv.tv_usec;
#if defined(_WINDOWS) || defined(_WIN32)
        *pSeed += (uint32_t)_getpid();
#else
        *pSeed += (uint32_t)getpid();
#endif
    }
}

int32_t random_rand32(uint32_t* pSeed)
{
    *pSeed = (*pSeed * 1103515245 + 12345) & 0x7fffffff;
    return (int32_t)(*pSeed);
}

int32_t random_rand(uint32_t* pSeed, int32_t iMax)
{
    int32_t iDivisor = INT32_MAX / iMax;
    int32_t iRet;
    do {
        iRet = random_rand32(pSeed) / iDivisor;
    } while (iRet >= iMax);
    return iRet;
}

struct poller_s
{
    // private
    fd_set*         pReadSetIn;
    fd_set*         pWriteSetIn;
    fd_set*         pReadSetOut;
    fd_set*         pWriteSetOut;
    size_t          nEventFdsSize;
    size_t          nNumFdsInFdSets;
    int32_t         iEventFds;
    bool            bResizeOutSets;
    pollHandle_tt** ppPollHandle;
    uint32_t        uiSeed;
    atomic_int      iRefCount;
};

poller_tt* createPoller()
{
    poller_tt* pPoller = (poller_tt*)mem_malloc(sizeof(poller_tt));

    size_t nSize = selectAllocSize(32 + 1);

    pPoller->pReadSetIn = (fd_set*)mem_malloc(nSize);
    bzero((void*)pPoller->pReadSetIn, nSize);

    pPoller->pWriteSetIn = (fd_set*)mem_malloc(nSize);
    bzero((void*)pPoller->pWriteSetIn, nSize);

    pPoller->pReadSetOut  = NULL;
    pPoller->pWriteSetOut = NULL;

    pPoller->nEventFdsSize   = nSize;
    pPoller->bResizeOutSets  = false;
    pPoller->nNumFdsInFdSets = 32;
    pPoller->iEventFds       = 0;
    pPoller->ppPollHandle    = mem_malloc(sizeof(struct pollHandle_s*) * pPoller->nNumFdsInFdSets);
    pPoller->uiSeed          = 0;
    random_seed(&pPoller->uiSeed);
    atomic_init(&pPoller->iRefCount, 1);
    return pPoller;
}

void poller_release(poller_tt* pPoller)
{
    if (atomic_fetch_sub(&(pPoller->iRefCount), 1) == 1) {
        mem_free(pPoller->pReadSetIn);
        mem_free(pPoller->pWriteSetIn);
        mem_free(pPoller->pReadSetOut);
        mem_free(pPoller->pWriteSetOut);
        mem_free(pPoller->ppPollHandle);
        mem_free(pPoller);
    }
}

int32_t poller_wait(poller_tt* pPoller, int32_t iTimeoutMs)
{
    if (pPoller->bResizeOutSets) {
        pPoller->pReadSetOut  = (fd_set*)mem_realloc(pPoller->pReadSetOut, pPoller->nEventFdsSize);
        pPoller->pWriteSetOut = (fd_set*)mem_realloc(pPoller->pWriteSetOut, pPoller->nEventFdsSize);
        pPoller->bResizeOutSets = false;
    }

    memcpy(pPoller->pReadSetOut, pPoller->pReadSetIn, pPoller->nEventFdsSize);
    memcpy(pPoller->pWriteSetOut, pPoller->pWriteSetIn, pPoller->nEventFdsSize);

    int32_t iCount = pPoller->iEventFds + 1;

    struct timeval* pTimeout = NULL;
    struct timeval  timeout;
    if (iTimeoutMs != -1) {
        timeout.tv_sec  = iTimeoutMs / 1000;
        timeout.tv_usec = (iTimeoutMs % 1000) * 1000;
        pTimeout        = &timeout;
    }

    int32_t iRet = select(iCount, pPoller->pReadSetOut, pPoller->pWriteSetOut, NULL, &timeout);

    if (iRet == -1) {
        if (errno != EINTR) {
            return -1;
        }
        return 0;
    }
    return iCount;
}

void poller_dispatch(poller_tt* pPoller, int32_t iEvents)
{
    int32_t iIndex = random_rand(&(pPoller->uiSeed), iEvents);

    int32_t iAttribute = 0;

    for (int32_t i = 0; i < iEvents; ++i) {
        if (iIndex >= iEvents) iIndex = 0;
        iAttribute = 0;
        if (FD_ISSET(iIndex, pPoller->pReadSetOut)) iAttribute |= ePollerReadable;
        if (FD_ISSET(iIndex, pPoller->pWriteSetOut)) iAttribute |= ePollerWritable;

        if (iAttribute == 0) continue;

        if (pPoller->ppPollHandle[iIndex]) {
            pPoller->ppPollHandle[iIndex]->fn(pPoller->ppPollHandle[iIndex], iAttribute);
        }
    }
}

void poller_add(poller_tt* pPoller, int32_t iSocket, struct pollHandle_s* pHandle,
                int32_t iAttribute, pollCallbackFunc fn)
{
    pHandle->fn         = fn;
    pHandle->iAttribute = iAttribute;
    int32_t fd          = iSocket;
    if (iSocket >= pPoller->nNumFdsInFdSets) {
        size_t fdsz = pPoller->nNumFdsInFdSets;
        while (fdsz <= fd + 1) fdsz *= 2;

        pPoller->ppPollHandle =
            mem_realloc(pPoller->ppPollHandle, sizeof(struct pollHandle_s*) * fdsz);
        pPoller->nNumFdsInFdSets = fdsz;
    }
    pPoller->ppPollHandle[fd] = pHandle;

    if (pPoller->iEventFds < fd) {
        size_t fdsz = pPoller->nEventFdsSize;

        if (fdsz < sizeof(fd_mask)) fdsz = sizeof(fd_mask);

        while (fdsz < selectAllocSize(fd + 1)) fdsz *= 2;

        if (fdsz != pPoller->nEventFdsSize) {
            pPoller->pReadSetIn     = (fd_set*)mem_realloc(pPoller->pReadSetIn, fdsz);
            pPoller->pWriteSetIn    = (fd_set*)mem_realloc(pPoller->pWriteSetIn, fdsz);
            pPoller->bResizeOutSets = true;

            bzero((char*)pPoller->pReadSetIn + pPoller->nEventFdsSize,
                  fdsz - pPoller->nEventFdsSize);
            bzero((char*)pPoller->pReadSetIn + pPoller->nEventFdsSize,
                  fdsz - pPoller->nEventFdsSize);

            pPoller->nEventFdsSize = fdsz;
        }
        pPoller->iEventFds = fd;
    }

    if (iAttribute & ePollerReadable) {
        FD_SET(fd, pPoller->pReadSetIn);
    }

    if (iAttribute & ePollerWritable) {
        FD_SET(fd, pPoller->pWriteSetIn);
    }
}

void poller_clear(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    if (pPoller->iEventFds < iSocket) {
        return;
    }

    if (iAttribute & ePollerClosed) {
        if (!(pHandle->iAttribute & ePollerClosed)) {
            if (pHandle->iAttribute & ePollerReadable) {
                FD_CLR(iSocket, pPoller->pReadSetIn);
                pHandle->iAttribute &= ~ePollerReadable;
            }

            if (pHandle->iAttribute & ePollerWritable) {
                FD_CLR(iSocket, pPoller->pWriteSetIn);
                pHandle->iAttribute &= ~ePollerWritable;
            }
            pollHandle_tt* pHandle = pPoller->ppPollHandle[iSocket];
            if (pHandle) {
                pPoller->ppPollHandle[iSocket] = NULL;
            }
            pHandle->iAttribute = ePollerClosed;
        }
    }
    else {
        if (iAttribute & ePollerReadable && pHandle->iAttribute & ePollerReadable) {
            FD_CLR(iSocket, pPoller->pReadSetIn);
            pHandle->iAttribute &= ~ePollerReadable;
        }

        if (iAttribute & ePollerWritable && pHandle->iAttribute & ePollerWritable) {
            FD_CLR(iSocket, pPoller->pWriteSetIn);
            pHandle->iAttribute &= ~ePollerWritable;
        }
    }
}

void poller_setOpt(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    if (iSocket > pPoller->iEventFds) {
        return;
    }

    if (iAttribute & ePollerReadable && !(pHandle->iAttribute & ePollerReadable)) {
        pHandle->iAttribute |= ePollerReadable;
        FD_SET(iSocket, pPoller->pReadSetIn);
    }

    if (iAttribute & ePollerWritable && !(pHandle->iAttribute & ePollerWritable)) {
        pHandle->iAttribute |= ePollerWritable;
        FD_SET(iSocket, pPoller->pWriteSetIn);
    }
}
