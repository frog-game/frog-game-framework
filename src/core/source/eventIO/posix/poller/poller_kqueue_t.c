

#include "eventIO/internal/posix/poller_t.h"
#include "log_t.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct poller_s
{
    // private
    struct kevent* pEvent;
    int32_t        iEventCount;
    int32_t        iKqueueFd;
    atomic_int     iRefCount;
};

poller_tt* createPoller()
{
    poller_tt* pPoller = (poller_tt*)mem_malloc(sizeof(poller_tt));

    pPoller->iKqueueFd = kqueue();
    if (pPoller->iKqueueFd < 0) {
        Log(eLog_error, "kqueue");
        return NULL;
    }
    pPoller->iEventCount = 128;
    pPoller->pEvent      = mem_malloc(pPoller->iEventCount * sizeof(struct kevent));
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

        if (pPoller->iKqueueFd >= 0) {
            close(pPoller->iKqueueFd);
            pPoller->iKqueueFd = -1;
        }
        mem_free(pPoller);
    }
}

int32_t poller_wait(poller_tt* pPoller, int32_t iTimeoutMs)
{
    struct timespec* pTimeout = NULL;
    struct timespec  timeout;
    if (iTimeoutMs != -1) {
        timeout.tv_sec  = iTimeoutMs / 1000;
        timeout.tv_nsec = (iTimeoutMs % 1000) * 1000000;
        pTimeout        = &timeout;
    }

    int32_t iEvents =
        kevent(pPoller->iKqueueFd, NULL, 0, pPoller->pEvent, pPoller->iEventCount, pTimeout);

    if (iEvents == -1) {
        int32_t iErrno = errno;
        if (iErrno != EINTR) {
            Log(eLog_error, "kevent");
            return -1;
        }
        return 0;
    }
    return iEvents;
}

void poller_dispatch(poller_tt* pPoller, int32_t iEvents)
{
    for (int32_t i = 0; i < iEvents; ++i) {
        int32_t iAttribute = 0;
        if (pPoller->pEvent[i].flags & EV_ERROR) {
            iAttribute |= ePollerClosed;
        }
        else if (pPoller->pEvent[i].filter == EVFILT_READ) {
            iAttribute |= ePollerReadable;
        }
        else if (pPoller->pEvent[i].filter == EVFILT_WRITE) {
            iAttribute |= ePollerWritable;
        }

        if (iAttribute == 0) {
            continue;
        }

        pollHandle_tt* pPollHandle = (pollHandle_tt*)(pPoller->pEvent[i].udata);
        pPollHandle->fn(pPollHandle, iAttribute);
    }

    if (iEvents == pPoller->iEventCount) {
        int32_t        iReallocEventCount = pPoller->iEventCount * 2;
        struct kevent* pNewEvents =
            mem_realloc(pPoller->pEvent, iReallocEventCount * sizeof(struct kevent));
        if (pNewEvents) {
            pPoller->pEvent      = pNewEvents;
            pPoller->iEventCount = iReallocEventCount;
        }
    }
}

void poller_add(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute,
                pollCallbackFunc fn)
{
    pHandle->fn         = fn;
    pHandle->iAttribute = iAttribute;

    struct kevent event;
    bzero(&event, sizeof event);
    EV_SET(&event, iSocket, EVFILT_READ, EV_ADD, 0, 0, pHandle);
    if (kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR) {
        Log(eLog_error, "poller_add EVFILT_READ");
        return;
    }
    EV_SET(&event, iSocket, EVFILT_WRITE, EV_ADD, 0, 0, pHandle);
    if (kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR) {
        bzero(&event, sizeof event);
        EV_SET(&event, iSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
        Log(eLog_error, "poller_add EVFILT_WRITE");
        return;
    }

    if (!(pHandle->iAttribute & ePollerReadable)) {
        bzero(&event, sizeof event);
        EV_SET(&event, iSocket, EVFILT_READ, EV_DISABLE, 0, 0, pHandle);
        if (kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR) {
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
            Log(eLog_error, "poller_add EV_DISABLE EVFILT_WRITE");
            return;
        }
    }
    if (!(pHandle->iAttribute & ePollerWritable)) {
        bzero(&event, sizeof event);
        EV_SET(&event, iSocket, EVFILT_WRITE, EV_DISABLE, 0, 0, pHandle);
        if (kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR) {
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
            Log(eLog_error, "poller_add EV_DISABLE EVFILT_WRITE");
            return;
        }
    }
}

void poller_clear(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    struct kevent event;
    if (iAttribute & ePollerClosed) {
        if (!(pHandle->iAttribute & ePollerClosed)) {
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);

            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
            pHandle->iAttribute = ePollerClosed;
        }
    }
    else {
        if (iAttribute & ePollerReadable && pHandle->iAttribute & ePollerReadable) {
            pHandle->iAttribute &= ~ePollerReadable;
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_READ, EV_DISABLE, 0, 0, pHandle);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
        }

        if (iAttribute & ePollerWritable && pHandle->iAttribute & ePollerWritable) {
            pHandle->iAttribute &= ~ePollerWritable;
            bzero(&event, sizeof event);
            EV_SET(&event, iSocket, EVFILT_WRITE, EV_DISABLE, 0, 0, pHandle);
            kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
        }
    }
}

void poller_setOpt(poller_tt* pPoller, int32_t iSocket, pollHandle_tt* pHandle, int32_t iAttribute)
{
    struct kevent event;
    if (iAttribute & ePollerReadable && !(pHandle->iAttribute & ePollerReadable)) {
        pHandle->iAttribute |= ePollerReadable;
        bzero(&event, sizeof event);
        EV_SET(&event, iSocket, EVFILT_READ, EV_ENABLE, 0, 0, pHandle);
        kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
    }

    if (iAttribute & ePollerWritable && !(pHandle->iAttribute & ePollerWritable)) {
        pHandle->iAttribute |= ePollerWritable;
        bzero(&event, sizeof event);
        EV_SET(&event, iSocket, EVFILT_WRITE, EV_ENABLE, 0, 0, pHandle);
        kevent(pPoller->iKqueueFd, &event, 1, NULL, 0, NULL);
    }
}
