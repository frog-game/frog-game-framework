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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include "cmocka.h"

#include <stdatomic.h>
#include <stdlib.h>
#include "thread_t.h"
#include "log_t.h"
#include "time_t.h"

int32_t iKqueueFd[5];
int32_t hSocket[2];

static void threadRunFunc(void* pArg)
{
    int32_t*       pk          = (int32_t*)pArg;
    int32_t        iEventCount = 32;
    struct kevent* pEvent      = malloc(iEventCount * sizeof(struct kevent));

    while (1) {
        int32_t iEvents = kevent(*pk, NULL, 0, pEvent, iEventCount, NULL);

        if (iEvents == -1) {
            int32_t iErrno = errno;
            if (iErrno != EINTR) {
                Log(eLog_error, "kevent error");
                return;
            }
            return;
        }

        char szBuf[128];
        if (recv(hSocket[1], szBuf, sizeof(szBuf), 0) > 0) {
            Log(eLog_error, "kevent");
        }
    }
}

void kqueue_test(void** state)
{

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, hSocket) < 0) {
        return;
    }


    thread_tt thread[5];
    for (int32_t i = 0; i < 5; ++i) {
        iKqueueFd[i] = kqueue();
        struct kevent event;
        bzero(&event, sizeof event);
        EV_SET(&event, hSocket[1], EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(iKqueueFd[i], &event, 1, NULL, 0, NULL) == -1 || event.flags & EV_ERROR) {
            Log(eLog_error, "poller_add EVFILT_READ");
            return;
        }
        assert_int_equal(thread_start(&thread[i], threadRunFunc, &(iKqueueFd[i])), eThreadSuccess);
    }

    timespec_tt timeSleep;
    timeSleep.iSec  = 1;
    timeSleep.iNsec = 0;
    sleep_for(&timeSleep);

    char szBuf[1] = {};
    send(hSocket[0], szBuf, sizeof(szBuf), 0);
    send(hSocket[0], szBuf, sizeof(szBuf), 0);
    send(hSocket[0], szBuf, sizeof(szBuf), 0);
    send(hSocket[0], szBuf, sizeof(szBuf), 0);
    send(hSocket[0], szBuf, sizeof(szBuf), 0);


    for (int32_t i = 0; i < 5; ++i) {
        thread_join(thread[i]);
    }
}
