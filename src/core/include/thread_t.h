#pragma once

#include <stdbool.h>
#include <stdint.h>


#include "platform_t.h"

#if defined(_WINDOWS) || defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX 1
#    endif
#    include <Windows.h>
#    include <emmintrin.h>   //_mm_pause

typedef HANDLE           thread_tt;
typedef CRITICAL_SECTION mutex_tt;
#    if _WIN32_WINNT >= 0x0600
typedef SRWLOCK rwlock_tt;
#    else
typedef struct rwlock_s
{
    uint32_t         uiReadersNum;
    CRITICAL_SECTION readersLock;
    HANDLE           hWriteSemaphore;
} rwlock_tt;
#    endif
typedef HANDLE             sem_tt;
typedef CONDITION_VARIABLE cond_tt;

typedef struct once_flag_s
{
    HANDLE hEvent;
    bool   bCall;
} once_flag_tt;

#    define ONCE_FLAG_INIT \
        {                  \
            NULL, false    \
        }
#else
#    if !defined(__MVS__)
#        include <semaphore.h>
#        include <sys/param.h> /* MAXHOSTNAMELEN on Linux and the BSDs */
#    endif
#    include <pthread.h>
#    include <signal.h>
#    if defined(__APPLE__) && defined(__MACH__)
#        include <TargetConditionals.h>
#        include <mach/mach.h>
#        include <mach/semaphore.h>
#        include <mach/task.h>

#    endif

typedef pthread_t        thread_tt;
typedef pthread_mutex_t  mutex_tt;
typedef pthread_rwlock_t rwlock_tt;
#    if defined(__APPLE__) && defined(__MACH__)
typedef semaphore_t      sem_tt;
#    else
typedef sem_t sem_tt;
#    endif

typedef pthread_cond_t cond_tt;
typedef pthread_once_t once_flag_tt;

#    define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#endif

enum
{
    eThreadSuccess  = 0,
    eThreadNomem    = 1,
    eThreadTimedout = 2,
    eThreadInvalid  = 3,
    eThreadError    = 4,
};

//----------------thread----------------
typedef void (*thread_func)(void* pArg);

frCore_API int32_t thread_start(thread_tt* pHandle, thread_func fn, void* pArg);

frCore_API void thread_join(thread_tt handle);

frCore_API thread_tt threadSelf();

frCore_API uint64_t threadId();

frCore_API bool threadEqual(const thread_tt handle1, const thread_tt handle2);

frCore_API void threadExit();

frCore_API void threadYield();

frCore_API uint32_t threadHardwareConcurrency();

frCore_API uint64_t getThreadClock();

//----------------call_once----------------
frCore_API void callOnce(once_flag_tt* pFlag, void (*func)());

//----------------tls----------------
typedef void (*tls_cleanup_func)(void* pArg);

frCore_API void setTlsValue(void const* pKey, tls_cleanup_func fn, void* pTlsData,
                            bool bExitCleanup);

frCore_API void* getTlsValue(void const* pKey);

//----------------mutex----------------
frCore_API bool mutex_init(mutex_tt* pMutex);

frCore_API void mutex_destroy(mutex_tt* pMutex);

frCore_API void mutex_lock(mutex_tt* pMutex);

frCore_API bool mutex_trylock(mutex_tt* pMutex);

frCore_API void mutex_unlock(mutex_tt* pMutex);

//----------------rwlock----------------
frCore_API bool rwlock_init(rwlock_tt* pRwlock);

frCore_API void rwlock_destroy(rwlock_tt* pRwlock);

frCore_API void rwlock_rdlock(rwlock_tt* pRwlock);

frCore_API bool rwlock_tryrdlock(rwlock_tt* pRwlock);

frCore_API void rwlock_rdunlock(rwlock_tt* pRwlock);

frCore_API void rwlock_wrlock(rwlock_tt* pRwlock);

frCore_API bool rwlock_trywrlock(rwlock_tt* pRwlock);

frCore_API void rwlock_wrunlock(rwlock_tt* pRwlock);

//----------------semaphore----------------
frCore_API bool sema_init(sem_tt* pSem, uint32_t uiValue);

frCore_API void sema_destroy(sem_tt* pSem);

frCore_API void sema_post(sem_tt* pSem);

frCore_API void sema_wait(sem_tt* pSem);

frCore_API bool sema_trywait(sem_tt* pSem);

//----------------cond----------------
frCore_API bool cond_init(cond_tt* pCond);

frCore_API void cond_destroy(cond_tt* pCond);

frCore_API void cond_signal(cond_tt* pCond);

frCore_API void cond_broadcast(cond_tt* pCond);

frCore_API void cond_wait(cond_tt* pCond, mutex_tt* pMutex);

frCore_API int32_t cond_timedwait(cond_tt* pCond, mutex_tt* pMutex, uint64_t uiTimeoutNs);

//----------------sleep----------------
struct timespec_s;

frCore_API bool sleep_for(const struct timespec_s* pDuration);

//----------------thread_pause----------------
static inline void thread_pause()
{
#ifdef _MSC_VER
    _mm_pause();
#elif defined(__clang__) || defined(__GNUC__)
#    if defined(__x86_64__) || defined(__i386__)
    asm volatile("pause" ::: "memory");
#    elif defined(__arm__) || defined(__aarch64__)
    asm volatile("yield");
#    endif
#else
    threadYield();
#endif
}