#include "thread_t.h"
#include "rbtree_t.h"
#include "time_t.h"
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#ifdef __GLIBC__
#    include <sys/sysinfo.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#    include <sys/time.h>
#    include <sys/types.h>
#    include <sys/sysctl.h>
#else
#    include <unistd.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#    include <mach/thread_act.h>
#else
#    include <time.h>
#endif

#if defined(__linux__)
#    include <unistd.h>
#    include <sys/syscall.h>

#endif

#ifdef __MVS__
#    include <sys/ipc.h>
#    include <sys/sem.h>
#endif

#ifdef __GLIBC__
#    include <gnu/libc-version.h> /* gnu_get_libc_version() */
#endif

#undef NANOSEC
#define NANOSEC ((uint64_t)1e9)

struct thread_data_t
{
    thread_func fn;
    void*       pArg;
};

static void* threadProxy(void* pArg)
{
    struct thread_data_t* pThreadData;
    struct thread_data_t  threadData;
    pThreadData = pArg;
    threadData  = *pThreadData;
    mem_free(pThreadData);
    threadData.fn(threadData.pArg);
    return NULL;
}

int32_t thread_start(thread_tt* pHandle, thread_func fn, void* pArg)
{
    struct thread_data_t* pThread_data = mem_malloc(sizeof(struct thread_data_t));
    if (pThread_data == NULL) {
        return eThreadNomem;
    }

    pThread_data->fn   = fn;
    pThread_data->pArg = pArg;

    const int32_t ret = pthread_create(pHandle, 0, threadProxy, pThread_data);
    switch (ret) {
    case 0:
    {
        return eThreadSuccess;
    }
    case EAGAIN:
    {
        mem_free(pThread_data);
        return eThreadNomem;
    }
    }
    mem_free(pThread_data);
    return eThreadError;
}

void thread_join(thread_tt handle)
{
    pthread_join(handle, NULL);
}

thread_tt threadSelf()
{
    return pthread_self();
}

uint64_t threadId()
{
#if defined(__APPLE__)
    uint64_t tid;
    (void)pthread_threadid_np(NULL, &tid);
    return tid;
#elif defined(__linux__)
    return syscall(SYS_gettid);
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

bool threadEqual(const thread_tt handle1, const thread_tt handle2)
{
    return pthread_equal(handle1, handle2) != 0;
}

void threadExit()
{
    pthread_exit(NULL);
}

void threadYield()
{
    // pthread_yield();
    sched_yield();
}

uint32_t threadHardwareConcurrency()
{
#if defined(__APPLE__) || defined(__FreeBSD__)
    int    count;
    size_t size = sizeof(count);
    return sysctlbyname("hw.ncpu", &count, &size, NULL, 0) ? 0 : count;
#elif defined(_SC_NPROCESSORS_ONLN)
    int const count = sysconf(_SC_NPROCESSORS_ONLN);
    return (count > 0) ? count : 0;
#elif defined(__GLIBC__)
    return get_nprocs();
#else
    return 0;
#endif
}

void callOnce(once_flag_tt* pFlag, void (*func)(void))
{
    if (pthread_once(pFlag, func)) {
        abort();
    }
}

typedef struct tlsDataNode_s
{
    RB_ENTRY(tlsDataNode_s)
    entry;

    void const*      pKey;
    tls_cleanup_func fn;
    void*            pTlsValue;
} tlsDataNode_tt;

static int32_t tlsDataNodeCmp(struct tlsDataNode_s* pNode1, struct tlsDataNode_s* pNode2)
{
    return (pNode1->pKey < pNode2->pKey ? -1 : pNode1->pKey > pNode2->pKey ? 1 : 0);
}

RB_HEAD(threadTlsData_s, tlsDataNode_s);
RB_GENERATE_STATIC(threadTlsData_s, tlsDataNode_s, entry, tlsDataNodeCmp)

static pthread_key_t s_tlsKey = 0;

static void tls_destructor(void* pData)
{
    if (pData) {
        struct threadTlsData_s* pThreadTlsData = (struct threadTlsData_s*)pData;

        struct tlsDataNode_s* pNextNode = NULL;

        for (struct tlsDataNode_s* pNode = RB_MIN(threadTlsData_s, pThreadTlsData);
             pNode != NULL;) {
            pNextNode = RB_NEXT(threadTlsData_s, pThreadTlsData, pNode);
            if (pNode->fn && pNode->pTlsValue) {
                pNode->fn(pNode->pTlsValue);
            }

            RB_REMOVE(threadTlsData_s, pThreadTlsData, pNode);
            mem_free(pNode);

            pNode = pNextNode;
        }
        mem_free(pThreadTlsData);
    }
}

static void createTlsKey()
{
    if (pthread_key_create(&s_tlsKey, tls_destructor)) {
        abort();
    }
}

// static void cleanupTlsKey()
// {
//     if(s_tlsKey !=0)
//     {
//         pthread_key_delete(s_tlsKey);
//         s_tlsKey = 0;
//     }
// }

static bool setThreadTlsData(struct threadTlsData_s* pNewData)
{
    static once_flag_tt s_current_thread_tls_init_flag = ONCE_FLAG_INIT;
    callOnce(&s_current_thread_tls_init_flag, createTlsKey);

    if (pthread_setspecific(s_tlsKey, pNewData)) {
        return false;
    }

    return true;
}

static struct threadTlsData_s* getThreadTlsData()
{
    if (s_tlsKey == 0) {
        return NULL;
    }
    return (struct threadTlsData_s*)pthread_getspecific(s_tlsKey);
}

static struct threadTlsData_s* getCurrentThreadData()
{
    struct threadTlsData_s* pThreadData = getThreadTlsData();
    if (pThreadData == NULL) {
        pThreadData = mem_malloc(sizeof(struct threadTlsData_s));
        RB_INIT(pThreadData);
        if (!setThreadTlsData(pThreadData)) {
            mem_free(pThreadData);
            pThreadData = NULL;
        }
    }
    return pThreadData;
}

static tlsDataNode_tt* findTlsDataNode(void const* pKey)
{
    struct threadTlsData_s* const pThreadData = getThreadTlsData();
    if (pThreadData) {
        tlsDataNode_tt dataNode;
        dataNode.pKey = pKey;
        return RB_FIND(threadTlsData_s, pThreadData, &dataNode);
    }
    return NULL;
}

void* getTlsValue(void const* pKey)
{
    tlsDataNode_tt* pNode = findTlsDataNode(pKey);
    if (pNode) {
        return pNode->pTlsValue;
    }
    return NULL;
}

static void addTlsDataNode(void const* pKey, tls_cleanup_func fn, void* pTlsData)
{
    struct threadTlsData_s* const pCurrentThreadData = getCurrentThreadData();
    tlsDataNode_tt*               pNode              = mem_malloc(sizeof(tlsDataNode_tt));
    pNode->pKey                                      = pKey;
    pNode->fn                                        = fn;
    pNode->pTlsValue                                 = pTlsData;
    RB_INSERT(threadTlsData_s, pCurrentThreadData, pNode);
}

static void eraseTlsDataNode(void const* pKey)
{
    struct threadTlsData_s* const pCurrentThreadData = getCurrentThreadData();

    tlsDataNode_tt dataNode;
    dataNode.pKey         = pKey;
    tlsDataNode_tt* pNode = RB_FIND(threadTlsData_s, pCurrentThreadData, &dataNode);
    if (pNode) {
        RB_REMOVE(threadTlsData_s, pCurrentThreadData, pNode);
        mem_free(pNode);
    }
}

void setTlsValue(void const* pKey, tls_cleanup_func fn, void* pTlsData, bool bExitCleanup)
{
    tlsDataNode_tt* const pCurrentNode = findTlsDataNode(pKey);
    if (pCurrentNode) {
        if (bExitCleanup && pCurrentNode->fn && (pCurrentNode->pTlsValue != NULL)) {
            pCurrentNode->fn(pCurrentNode->pTlsValue);
        }
        if (fn || (pTlsData != NULL)) {
            pCurrentNode->fn        = fn;
            pCurrentNode->pTlsValue = pTlsData;
        }
        else {
            eraseTlsDataNode(pKey);
        }
    }
    else if (fn || (pTlsData != NULL)) {
        addTlsDataNode(pKey, fn, pTlsData);
    }
}

// void onTlsProcessExit()
// {
// 	cleanupTlsKey();
// }

bool mutex_init(mutex_tt* pMutex)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr)) {
        abort();
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)) {
        abort();
    }
    int32_t iError = pthread_mutex_init(pMutex, &attr);

    if (pthread_mutexattr_destroy(&attr)) {
        abort();
    }
    return iError == 0;
}

void mutex_destroy(mutex_tt* pMutex)
{
    if (pthread_mutex_destroy(pMutex)) {
        abort();
    }
    pMutex = NULL;
}

void mutex_lock(mutex_tt* pMutex)
{
    if (pthread_mutex_lock(pMutex)) {
        abort();
    }
}

bool mutex_trylock(mutex_tt* pMutex)
{
    int32_t iError = pthread_mutex_trylock(pMutex);
    if (iError) {
        if (iError != EBUSY && iError != EAGAIN) {
            abort();
        }
        return false;
    }
    return true;
}

void mutex_unlock(mutex_tt* pMutex)
{
    if (pthread_mutex_unlock(pMutex)) {
        abort();
    }
}

bool rwlock_init(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_init(pRwlock, NULL)) {
        return false;
    }
    return true;
}

void rwlock_destroy(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_destroy(pRwlock)) {
        abort();
    }
    pRwlock = NULL;
}

void rwlock_rdlock(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_rdlock(pRwlock)) {
        abort();
    }
}

bool rwlock_tryrdlock(rwlock_tt* pRwlock)
{
    int32_t iError = pthread_rwlock_tryrdlock(pRwlock);
    if (iError) {
        if (iError != EBUSY && iError != EAGAIN) {
            abort();
        }
        return false;
    }
    return true;
}

void rwlock_rdunlock(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_unlock(pRwlock)) {
        abort();
    }
}

void rwlock_wrlock(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_wrlock(pRwlock)) {
        abort();
    }
}

bool rwlock_trywrlock(rwlock_tt* pRwlock)
{
    int32_t iError = pthread_rwlock_trywrlock(pRwlock);
    if (iError) {
        if (iError != EBUSY && iError != EAGAIN) {
            abort();
        }
        return false;
    }
    return true;
}

void rwlock_wrunlock(rwlock_tt* pRwlock)
{
    if (pthread_rwlock_unlock(pRwlock)) {
        abort();
    }
}

#if !defined(__APPLE__) || !defined(__MACH__)
#    ifdef __GLIBC__
static once_flag_tt glibc_version_check_once        = ONCE_FLAG_INIT;
static int32_t      platform_needs_custom_semaphore = 0;

static void glibc_version_check(void)
{
    const char* version = gnu_get_libc_version();
    platform_needs_custom_semaphore =
        version[0] == '2' && version[1] == '.' && atoi(version + 2) < 21;
}
#    elif defined(__MVS__)
#        define platform_needs_custom_semaphore 1
#    else /* !defined(__GLIBC__) && !defined(__MVS__) */
#        define platform_needs_custom_semaphore 0
#    endif

typedef struct __semaphore_s
{
    mutex_tt mutex;
    cond_tt  cond;
    uint32_t uiValue;
} __semaphore_t;

#    if defined(__GLIBC__) || platform_needs_custom_semaphore
static_assert(sizeof(sem_tt) >= sizeof(__semaphore_t*), "sizeof(sem_tt) >= sizeof(__semaphore_t*)");
#    endif

static bool __custom_sem_init(sem_tt* pSem, uint32_t uiValue)
{
    __semaphore_t* pSemaphore = mem_malloc(sizeof(__semaphore_t));
    if (pSemaphore == NULL) {
        return false;
    }

    if (!mutex_init(&pSemaphore->mutex)) {
        return false;
    }

    if (!cond_init(&pSemaphore->cond)) {
        return false;
    }
    pSemaphore->uiValue    = uiValue;
    *(__semaphore_t**)pSem = pSemaphore;
    return true;
}

static void __custom_sem_destroy(sem_tt* pSem)
{
    __semaphore_t* pSemaphore = *(__semaphore_t**)pSem;
    cond_destroy(&pSemaphore->cond);
    mutex_destroy(&pSemaphore->mutex);
    mem_free(pSemaphore);
}

static void __custom_sem_post(sem_tt* pSem)
{
    __semaphore_t* pSemaphore = *(__semaphore_t**)pSem;
    mutex_lock(&pSemaphore->mutex);
    pSemaphore->uiValue++;
    if (pSemaphore->uiValue == 1) {
        cond_signal(&pSemaphore->cond);
    }
    mutex_unlock(&pSemaphore->mutex);
}

static void __custom_sem_wait(sem_tt* pSem)
{
    __semaphore_t* pSemaphore = *(__semaphore_t**)pSem;
    mutex_lock(&pSemaphore->mutex);
    while (pSemaphore->uiValue == 0) {
        cond_wait(&pSemaphore->cond, &pSemaphore->mutex);
    }
    pSemaphore->uiValue--;
    mutex_unlock(&pSemaphore->mutex);
}

static bool __custom_sem_trywait(sem_tt* pSem)
{
    __semaphore_t* pSemaphore = *(__semaphore_t**)pSem;
    if (mutex_trylock(&pSemaphore->mutex) != 0) {
        return false;
    }

    if (pSemaphore->uiValue == 0) {
        mutex_unlock(&pSemaphore->mutex);
        return false;
    }

    pSemaphore->uiValue--;
    mutex_unlock(&pSemaphore->mutex);
    return true;
}

static bool __sem_init(sem_tt* pSem, uint32_t uiValue)
{
    if (sem_init(pSem, 0, uiValue)) {
        return false;
    }
    return true;
}

static void __sem_destroy(sem_tt* pSem)
{
    if (sem_destroy(pSem)) {
        abort();
    }
}

static void __sem_post(sem_tt* pSem)
{
    if (sem_post(pSem)) {
        abort();
    }
}

static void __sem_wait(sem_tt* pSem)
{
    int32_t iError = 0;

    do {
        iError = sem_wait(pSem);
    } while (iError == -1 && errno == EINTR);

    if (iError) {
        abort();
    }
}

static bool __sem_trywait(sem_tt* pSem)
{
    int32_t iError = 0;
    do {
        iError = sem_trywait(pSem);
    } while (iError == -1 && errno == EINTR);

    if (iError) {
        if (errno == EAGAIN) {
            return false;
        }
        abort();
    }

    return true;
}

#endif

bool sema_init(sem_tt* pSem, uint32_t uiValue)
{
#if defined(__APPLE__) && defined(__MACH__)
    if (semaphore_create(mach_task_self(), pSem, SYNC_POLICY_FIFO, uiValue) != KERN_SUCCESS) {
        return false;
    }
    return true;
#else
#    ifdef __GLIBC__
    callOnce(&glibc_version_check_once, glibc_version_check);
#    endif
    if (platform_needs_custom_semaphore) {
        return __custom_sem_init(pSem, uiValue);
    }
    else {
        return __sem_init(pSem, uiValue);
    }
#endif
}

void sema_destroy(sem_tt* pSem)
{
#if defined(__APPLE__) && defined(__MACH__)
    if (semaphore_destroy(mach_task_self(), *pSem)) {
        abort();
    }
#else
    if (platform_needs_custom_semaphore) {
        __custom_sem_destroy(pSem);
    }
    else {
        __sem_destroy(pSem);
    }
#endif
    pSem = NULL;
}

void sema_post(sem_tt* pSem)
{
#if defined(__APPLE__) && defined(__MACH__)
    if (semaphore_signal(*pSem)) {
        abort();
    }
#else
    if (platform_needs_custom_semaphore) {
        __custom_sem_post(pSem);
    }
    else {
        __sem_post(pSem);
    }
#endif
}

void sema_wait(sem_tt* pSem)
{
#if defined(__APPLE__) && defined(__MACH__)
    int32_t iError = 0;
    do {
        iError = semaphore_wait(*pSem);
    } while (iError == KERN_ABORTED);

    if (iError != KERN_SUCCESS) {
        abort();
    }
#else
    if (platform_needs_custom_semaphore) {
        __custom_sem_wait(pSem);
    }
    else {
        __sem_wait(pSem);
    }
#endif
}

bool sema_trywait(sem_tt* pSem)
{
#if defined(__APPLE__) && defined(__MACH__)
    mach_timespec_t interval;
    kern_return_t   iError;

    interval.tv_sec  = 0;
    interval.tv_nsec = 0;

    iError = semaphore_timedwait(*pSem, interval);
    if (iError == KERN_SUCCESS) {
        return true;
    }
    else if (iError == KERN_OPERATION_TIMED_OUT) {
        return false;
    }
    abort();
    return false;
#else
    if (platform_needs_custom_semaphore) {
        return __custom_sem_trywait(pSem);
    }
    else {
        return __sem_trywait(pSem);
    }
#endif
}

bool cond_init(cond_tt* pCond)
{
#if defined(__APPLE__) && defined(__MACH__) || defined(__MVS__)
    if (pthread_cond_init(pCond, NULL)) {
        return false;
    }
#else
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) {
        return false;
    }
#    if !(defined(__ANDROID_API__) && __ANDROID_API__ < 21)
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
        pthread_condattr_destroy(&attr);
        return false;
    }
#    endif

    if (pthread_cond_init(pCond, &attr)) {
        pthread_condattr_destroy(&attr);
        return false;
    }

    if (pthread_condattr_destroy(&attr)) {
        pthread_cond_destroy(pCond);
        return false;
    }
#endif
    return true;
}

void cond_destroy(cond_tt* pCond)
{
#if defined(__APPLE__) && defined(__MACH__)
    pthread_mutex_t mutex;

    if (pthread_mutex_init(&mutex, NULL)) {
        abort();
    }

    if (pthread_mutex_lock(&mutex)) {
        abort();
    }

    struct timespec timer;
    timer.tv_sec  = 0;
    timer.tv_nsec = 1;

    int32_t iError = pthread_cond_timedwait_relative_np(pCond, &mutex, &timer);
    if (iError != 0 && iError != ETIMEDOUT) {
        abort();
    }

    if (pthread_mutex_unlock(&mutex)) {
        abort();
    }

    if (pthread_mutex_destroy(&mutex)) {
        abort();
    }
#endif

    if (pthread_cond_destroy(pCond)) {
        abort();
    }
}

void cond_signal(cond_tt* pCond)
{
    if (pthread_cond_signal(pCond)) {
        abort();
    }
}

void cond_broadcast(cond_tt* pCond)
{
    if (pthread_cond_broadcast(pCond)) {
        abort();
    }
}

void cond_wait(cond_tt* pCond, mutex_tt* pMutex)
{
    if (pthread_cond_wait(pCond, pMutex)) {
        abort();
    }
}

int32_t cond_timedwait(cond_tt* pCond, mutex_tt* pMutex, uint64_t uiTimeoutNs)
{
    int32_t         r = 0;
    struct timespec timer;
#if defined(__MVS__)
    struct timeval tv;
#endif

#if defined(__APPLE__) && defined(__MACH__)
    timer.tv_sec  = uiTimeoutNs / NANOSEC;
    timer.tv_nsec = uiTimeoutNs % NANOSEC;
    r             = pthread_cond_timedwait_relative_np(pCond, pMutex, &timer);
#else
#    if defined(__MVS__)
    if (gettimeofday(&tv, NULL)) {
        abort();
    }
    uiTimeoutNs += tv.tv_sec * NANOSEC + tv.tv_usec * 1e3;
#    else
    timespec_tt now;
    getClockMonotonic(&now);
    uiTimeoutNs += timespec_toNsec(&now);
#    endif
    timer.tv_sec  = uiTimeoutNs / NANOSEC;
    timer.tv_nsec = uiTimeoutNs % NANOSEC;
#    if defined(__ANDROID_API__) && __ANDROID_API__ < 21

    r = pthread_cond_timedwait_monotonic_np(pCond, pMutex, &timer);
#    else
    r = pthread_cond_timedwait(pCond, pMutex, &timer);
#    endif
#endif

    if (r == 0) {
        return eThreadSuccess;
    }
    if (r == ETIMEDOUT) {
        return eThreadTimedout;
    }
    abort();
#ifndef __SUNPRO_C
    return eThreadError;
#endif
}

bool sleep_for(const struct timespec_s* pDuration)
{
    struct timespec timer = {pDuration->iSec, pDuration->iNsec};
    return nanosleep(&timer, 0) == 0;
}

uint64_t getThreadClock()
{
#if defined(__APPLE__) && defined(__MACH__)
    mach_port_t port = pthread_mach_thread_np(pthread_self());

    thread_basic_info_data_t info;
    mach_msg_type_number_t   count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(port, THREAD_BASIC_INFO, (thread_info_t)&info, &count) != KERN_SUCCESS) {
        return 0;
    }

    uint64_t uiUser = (uint64_t)(info.user_time.seconds) * 1000000000 +
                      (uint64_t)(info.user_time.microseconds) * 1000;
    uint64_t uiSystem = (uint64_t)(info.system_time.seconds) * 1000000000 +
                        (uint64_t)(info.system_time.microseconds) * 1000;

    return uiUser + uiSystem;
#else
    struct timespec ts;
#    if defined CLOCK_THREAD_CPUTIME_ID
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts))
#    else
    clockid_t clock_id;
    pthread_getcpuclockid(pthread_self(), &clock_id);
    if (clock_gettime(clock_id, &ts))
#    endif
    {
        return 0;
    }
    return (uint64_t)(ts.tv_sec) * 1000000000 + ts.tv_nsec;
#endif
}
