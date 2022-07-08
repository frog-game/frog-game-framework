

#include "eventIO/internal/posix/poller_t.h"
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
#include <sys/epoll.h>

struct poller_s
{
    // private
    struct epoll_event* pEvent;
    int32_t             iEventCount;
    int32_t             iEpollFd;
    atomic_int          iRefCount;
};

poller_tt* createPoller()
{
    poller_tt* pPoller = (poller_tt*)mem_malloc(sizeof(poller_tt));

    pPoller->iEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (pPoller->iEpollFd < 0) {
        Log(eLog_error, "epoll_create1");
        return NULL;
    }
    pPoller->iEventCount = 128;
    pPoller->pEvent      = mem_malloc(pPoller->iEventCount * sizeof(struct epoll_event));
    atomic_init(&pPoller->iRefCount, 1);
    return pPoller;
}

void poller_release(poller_tt* pPoller)
{
    if (atomic_fetch_sub(&(pPoller->iRefCount), 1) == 1) {
        if (pPoller->pEvent) {
            mem_free(pPoller->pEvent);
            pPoller->pEvent = NULL;
        }

        if (pPoller->iEpollFd >= 0) {
            close(pPoller->iEpollFd);
            pPoller->iEpollFd = -1;
        }
        mem_free(pPoller);
    }
}

int32_t poller_wait(poller_tt* pPoller, int32_t iTimeoutMs)
{
    int32_t iEvents =
        epoll_wait(pPoller->iEpollFd, pPoller->pEvent, pPoller->iEventCount, iTimeoutMs);
    if (iEvents == -1) {
        int32_t iErrno = errno;
        if (iErrno != EINTR) {
            return -1;
        }
        return 0;
    }
    return iEvents;
}

void poller_dispatch(poller_tt* pPoller, int32_t iEvents)
{
    for (int32_t i = 0; i < iEvents; ++i) {
        int16_t iEpollEvents = pPoller->pEvent[i].events;
        int32_t iAttribute   = 0;

        if (iEpollEvents & (EPOLLHUP | EPOLLERR)) {
            iAttribute = ePollerReadable | ePollerWritable;
        }
        else {
            if (iEpollEvents & EPOLLIN) iAttribute |= ePollerReadable;
            if (iEpollEvents & EPOLLOUT) iAttribute |= ePollerWritable;
            if (iEpollEvents & EPOLLRDHUP) iAttribute |= ePollerClosed;
        }

        if (iAttribute == 0) {
            continue;
        }
        pollHandle_tt* pPollHandle = (pollHandle_tt*)(pPoller->pEvent[i].data.ptr);
        pPollHandle->fn(pPollHandle, iAttribute);
    }

    if (iEvents == pPoller->iEventCount) {
        int32_t             iNewEventCount = pPoller->iEventCount * 2;
        struct epoll_event* pNewEvents =
            mem_realloc(pPoller->pEvent, iNewEventCount * sizeof(struct epoll_event));
        if (pNewEvents) {
            pPoller->pEvent      = pNewEvents;
            pPoller->iEventCount = iNewEventCount;
        }
    }
}

void poller_add(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute,
                pollCallbackFunc fn)
{
    pHandle->fn         = fn;
    pHandle->iAttribute = iAttribute;
    int32_t iEvents     = 0;

    if (pHandle->iAttribute & ePollerReadable) {
        iEvents = EPOLLIN;
    }

    if (pHandle->iAttribute & ePollerWritable) {
        iEvents |= EPOLLOUT;
    }

    struct epoll_event event;
    bzero(&event, sizeof event);
    event.events   = iEvents;
    event.data.ptr = pHandle;
    if (epoll_ctl(pPoller->iEpollFd, EPOLL_CTL_ADD, iSocket, &event) < 0) {
        Log(eLog_error, "epoll_ctl EPOLL_CTL_ADD");
    }
}

void poller_clear(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    if (iAttribute & ePollerClosed) {
        if (!(pHandle->iAttribute & ePollerClosed)) {
            if (epoll_ctl(pPoller->iEpollFd, EPOLL_CTL_DEL, iSocket, NULL) < 0) {
                Log(eLog_error, "epoll_ctl EPOLL_CTL_DEL");
            }
            pHandle->iAttribute = ePollerClosed;
        }
    }
    else {
        bool    bMod    = false;
        int32_t iEvents = 0;
        if (pHandle->iAttribute & ePollerReadable) {
            if (iAttribute & ePollerReadable) {
                pHandle->iAttribute &= ~ePollerReadable;
                bMod = true;
            }
            else {
                iEvents = EPOLLIN;
            }
        }

        if (pHandle->iAttribute & ePollerWritable) {
            if (iAttribute & ePollerWritable) {
                pHandle->iAttribute &= ~ePollerWritable;
                bMod = true;
            }
            else {
                iEvents = EPOLLOUT;
            }
        }

        if (bMod) {
            struct epoll_event event;
            bzero(&event, sizeof event);
            event.events   = iEvents;
            event.data.ptr = pHandle;
            if (epoll_ctl(pPoller->iEpollFd, EPOLL_CTL_MOD, iSocket, &event) < 0) {
                Log(eLog_error, "epoll_ctl EPOLL_CTL_MOD");
            }
        }
    }
}

void poller_setOpt(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    int32_t iEvents = 0;
    bool    bMod    = false;
    if (pHandle->iAttribute & ePollerReadable) {
        iEvents = EPOLLIN;
    }
    else {
        if (iAttribute & ePollerReadable) {
            pHandle->iAttribute |= ePollerReadable;
            iEvents = EPOLLIN;
            bMod    = true;
        }
    }

    if (pHandle->iAttribute & ePollerWritable) {
        iEvents |= EPOLLOUT;
    }
    else {
        if (iAttribute & ePollerWritable) {
            pHandle->iAttribute |= ePollerWritable;
            iEvents |= EPOLLOUT;
            bMod = true;
        }
    }

    if (bMod) {
        struct epoll_event event;
        bzero(&event, sizeof event);
        event.events   = iEvents;
        event.data.ptr = pHandle;
        if (epoll_ctl(pPoller->iEpollFd, EPOLL_CTL_MOD, iSocket, &event) < 0) {
            Log(eLog_error, "epoll_ctl EPOLL_CTL_MOD");
        }
    }
}
