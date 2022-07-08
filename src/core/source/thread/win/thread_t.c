#include "thread_t.h"
#include "rbtree_t.h"
#include "time_t.h"
#include <assert.h>
#include <errno.h>

#ifndef UNDER_CE
#    include <process.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <limits.h>

static void once_full(once_flag_tt* pFlag, void (*func)())
{
    HANDLE hEvent = CreateEvent(NULL, 1, 0, NULL);
    assert(hEvent && "CreateEvent");

    HANDLE hInitTargetEvent = InterlockedCompareExchangePointer(&pFlag->hEvent, hEvent, NULL);
    if (hInitTargetEvent == NULL) {
        func();
        SetEvent(hEvent);
        pFlag->bCall = true;
    }
    else {
        CloseHandle(hEvent);
        WaitForSingleObject(hInitTargetEvent, INFINITE);
    }
}

void callOnce(once_flag_tt* pFlag, void (*func)())
{
    if (pFlag->bCall) {
        return;
    }
    once_full(pFlag, func);
}

#if defined(UNDER_CE)
#    define FLS_OUT_OF_INDEXES 0xFFFFFFFF
#endif

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

typedef struct threadTlsData_s
{
    struct tlsDataNode_s* rbh_root; /* root of the tree */
    HANDLE                hEventHandle;
} threadTlsData_tt;

// RB_HEAD(threadTlsData_s, tlsDataNode_s);
RB_GENERATE_STATIC(threadTlsData_s, tlsDataNode_s, entry, tlsDataNodeCmp)

static DWORD s_uiTlsKey = FLS_OUT_OF_INDEXES;

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

        CloseHandle(pThreadTlsData->hEventHandle);
        mem_free(pThreadTlsData);
    }
}

static void createTlsKey()
{
    s_uiTlsKey = FlsAlloc(tls_destructor);
    assert(s_uiTlsKey != FLS_OUT_OF_INDEXES);
}

// static void cleanupTlsKey()
// {
// 	if(s_uiTlsKey!=FLS_OUT_OF_INDEXES)
// 	{
// 		FlsFree(s_uiTlsKey);
// 		s_uiTlsKey=FLS_OUT_OF_INDEXES;
// 	}
// }

static bool setThreadTlsData(struct threadTlsData_s* pNewData)
{
    static once_flag_tt s_current_thread_tls_init_flag = ONCE_FLAG_INIT;
    callOnce(&s_current_thread_tls_init_flag, createTlsKey);

    if (s_uiTlsKey != FLS_OUT_OF_INDEXES) {
        if (!FlsSetValue(s_uiTlsKey, pNewData)) {
            return false;
        }
    }
    else {
        return false;
    }
    return true;
}

static struct threadTlsData_s* getThreadTlsData()
{
    if (s_uiTlsKey == TLS_OUT_OF_INDEXES) {
        return NULL;
    }
    return (struct threadTlsData_s*)FlsGetValue(s_uiTlsKey);
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
        pThreadData->hEventHandle = CreateEvent(NULL, 1, 0, NULL);
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

struct thread_data_t
{
    thread_func fn;
    void*       pArg;
    thread_tt   self;
};

static uint32_t __stdcall threadProxy(void* pArg)
{
    struct thread_data_t* pThreadData;
    struct thread_data_t  threadData;

    pThreadData = pArg;
    threadData  = *pThreadData;
    mem_free(pThreadData);

    setTlsValue(&s_uiTlsKey, NULL, (void*)threadData.self, false);
    threadData.fn(threadData.pArg);
    return 0;
}

int32_t thread_start(thread_tt* pHandle, thread_func fn, void* pArg)
{
    struct thread_data_t* pThread_data = mem_malloc(sizeof(struct thread_data_t));

    if (pThread_data == NULL) {
        return eThreadNomem;
    }

    pThread_data->fn   = fn;
    pThread_data->pArg = pArg;

    HANDLE hThread =
        (HANDLE)_beginthreadex(NULL, 0, threadProxy, pThread_data, CREATE_SUSPENDED, NULL);

    if (hThread == NULL) {
        int32_t iError = errno;
        mem_free(pThread_data);

        if (iError == EAGAIN) {
            return eThreadNomem;
        }
        else {
            return eThreadError;
        }
    }

    *pHandle           = hThread;
    pThread_data->self = hThread;
    ResumeThread(hThread);
    return eThreadSuccess;
}

void thread_join(thread_tt handle)
{
    WaitForSingleObject(handle, INFINITE);
}

thread_tt threadSelf()
{
    return (thread_tt)getTlsValue(&s_uiTlsKey);
}

uint64_t threadId()
{
    return GetCurrentThreadId();
}

bool threadEqual(const thread_tt handle1, const thread_tt handle2)
{
    return handle1 == handle2;
}

void threadExit()
{
    ExitThread(0);
}

void threadYield()
{
    Sleep(0);
}

uint32_t threadHardwareConcurrency()
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}

bool mutex_init(mutex_tt* pMutex)
{
    InitializeCriticalSection(pMutex);
    return true;
}

void mutex_destroy(mutex_tt* pMutex)
{
    DeleteCriticalSection(pMutex);
    pMutex = NULL;
}

void mutex_lock(mutex_tt* pMutex)
{
    EnterCriticalSection(pMutex);
}

bool mutex_trylock(mutex_tt* pMutex)
{
    return TryEnterCriticalSection(pMutex);
}

void mutex_unlock(mutex_tt* pMutex)
{
    LeaveCriticalSection(pMutex);
}

bool rwlock_init(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    InitializeSRWLock(pRwlock);
#else
    HANDLE hHandle = CreateSemaphore(NULL, 1, 1, NULL);
    if (hHandle == NULL) {
        return false;
    }
    pRwlock->hWriteSemaphore = hHandle;

    InitializeCriticalSection(&pRwlock->readersLock);

    pRwlock->uiReadersNum = 0;
#endif
    return true;
}

void rwlock_destroy(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT < 0x0600
    DeleteCriticalSection(&pRwlock->readersLock);
    CloseHandle(pRwlock->hWriteSemaphore);
#endif
    pRwlock = NULL;
}

void rwlock_rdlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    AcquireSRWLockShared(pRwlock);
#else
    EnterCriticalSection(&pRwlock->readersLock);

    if (++pRwlock->uiReadersNum == 1) {
        WaitForSingleObject(pRwlock->hWriteSemaphore, INFINITE);
    }

    LeaveCriticalSection(&pRwlock->readersLock);
#endif
}

bool rwlock_tryrdlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    return (bool)TryAcquireSRWLockShared(pRwlock);
#else
    if (!TryEnterCriticalSection(&pRwlock->readersLock)) {
        return false;
    }

    bool bLock = true;

    if (pRwlock->uiReadersNum == 0) {
        DWORD r = WaitForSingleObject(pRwlock->hWriteSemaphore, 0);
        if (r == WAIT_OBJECT_0) {
            pRwlock->uiReadersNum++;
        }
        else if (r == WAIT_TIMEOUT) {
            bLock = false;
        }
        else if (r == WAIT_FAILED) {
            bLock = false;
        }
    }
    else {
        pRwlock->uiReadersNum++;
    }

    LeaveCriticalSection(&pRwlock->readersLock);
    return bLock;
#endif
}

void rwlock_rdunlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    ReleaseSRWLockShared(pRwlock);
#else
    EnterCriticalSection(&pRwlock->readersLock);

    if (--pRwlock->uiReadersNum == 0) {
        if (!ReleaseSemaphore(pRwlock->hWriteSemaphore, 1, NULL)) {
            abort();
        }
    }

    LeaveCriticalSection(&pRwlock->readersLock);
#endif
}

void rwlock_wrlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    AcquireSRWLockExclusive(pRwlock);
#else
    WaitForSingleObject(pRwlock->hWriteSemaphore, INFINITE);
#endif
}

bool rwlock_trywrlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    return (bool)TryAcquireSRWLockExclusive(pRwlock);
#else
    DWORD r = WaitForSingleObject(pRwlock->hWriteSemaphore, 0);
    if (r == WAIT_OBJECT_0) {
        return true;
    }
    return false;
#endif
}

void rwlock_wrunlock(rwlock_tt* pRwlock)
{
#if _WIN32_WINNT >= 0x0600
    ReleaseSRWLockExclusive(pRwlock);
#else
    if (!ReleaseSemaphore(pRwlock->hWriteSemaphore, 1, NULL)) {
        abort();
    }
#endif
}

bool sema_init(sem_tt* pSem, uint32_t uiValue)
{
    *pSem = CreateSemaphore(NULL, uiValue, INT_MAX, NULL);
    if (*pSem == NULL) {
        return false;
    }
    return true;
}

void sema_destroy(sem_tt* pSem)
{
    if (!CloseHandle(*pSem)) {
        abort();
    }
    pSem = NULL;
}

void sema_post(sem_tt* pSem)
{
    if (!ReleaseSemaphore(*pSem, 1, NULL)) {
        abort();
    }
}

void sema_wait(sem_tt* pSem)
{
    if (WaitForSingleObject(*pSem, INFINITE) != WAIT_OBJECT_0) {
        abort();
    }
}

bool sema_trywait(sem_tt* pSem)
{
    DWORD r = WaitForSingleObject(*pSem, 0);

    if (r == WAIT_OBJECT_0) {
        return true;
    }
    return false;
}

bool cond_init(cond_tt* pCond)
{
    InitializeConditionVariable(pCond);
    return true;
}

void cond_destroy(cond_tt* pCond)
{
    pCond = NULL;
}

void cond_signal(cond_tt* pCond)
{
    WakeConditionVariable(pCond);
}

void cond_broadcast(cond_tt* pCond)
{
    WakeAllConditionVariable(pCond);
}

void cond_wait(cond_tt* pCond, mutex_tt* pMutex)
{
    if (!SleepConditionVariableCS(pCond, pMutex, INFINITE)) {
        abort();
    }
}

int32_t cond_timedwait(cond_tt* pCond, mutex_tt* pMutex, uint64_t uiTimeoutNs)
{
    if (SleepConditionVariableCS(pCond, pMutex, (DWORD)(uiTimeoutNs / 1e6))) {
        return eThreadSuccess;
    }
    else if (GetLastError() != ERROR_TIMEOUT) {
        return eThreadError;
    }
    return eThreadTimedout;
}

// typedef struct _REASON_CONTEXT {
// 	ULONG Version;
// 	DWORD Flags;
// 	union {
// 		LPWSTR SimpleReasonString;
// 		struct {
// 			HMODULE LocalizedReasonModule;
// 			ULONG   LocalizedReasonId;
// 			ULONG   ReasonStringCount;
// 			LPWSTR  *ReasonStrings;
// 		} Detailed;
// 	} Reason;
// } REASON_CONTEXT, *PREASON_CONTEXT;
typedef BOOL(WINAPI* setwaitabletimerex_tt)(HANDLE, const LARGE_INTEGER*, LONG, PTIMERAPCROUTINE,
                                            LPVOID, PREASON_CONTEXT, ULONG);

static inline BOOL WINAPI SetWaitableTimerEx_emulation(HANDLE               hTimer,
                                                       const LARGE_INTEGER* lpDueTime, LONG lPeriod,
                                                       PTIMERAPCROUTINE pfnCompletionRoutine,
                                                       LPVOID           lpArgToCompletionRoutine,
                                                       PREASON_CONTEXT  WakeContext,
                                                       ULONG            TolerableDelay)
{
    return SetWaitableTimer(
        hTimer, lpDueTime, lPeriod, pfnCompletionRoutine, lpArgToCompletionRoutine, FALSE);
}

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 6387)   // MSVC sanitiser warns that GetModuleHandleA() might fail
#endif
static inline setwaitabletimerex_tt getSetWaitableTimerEx()
{
    static setwaitabletimerex_tt setwaitabletimerex_impl;
    if (setwaitabletimerex_impl) return setwaitabletimerex_impl;
    void (*addr)() =
        (void (*)())GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "SetWaitableTimerEx");
    if (addr)
        setwaitabletimerex_impl = (setwaitabletimerex_tt)addr;
    else
        setwaitabletimerex_impl = &SetWaitableTimerEx_emulation;
    return setwaitabletimerex_impl;
}
#ifdef _MSC_VER
#    pragma warning(pop)
#endif

bool sleep_for(const timespec_tt* pDuration)
{
    timespec_tt now;
    getClockMonotonic(&now);
    // timespec_tt timeout;
    // timespec_add(&now, pDuration, &timeout);

    int64_t timeoutNs = timespec_addToNs(&now, pDuration);

    HANDLE   handles[4]         = {0};
    unsigned handle_count       = 0;
    unsigned wait_handle_index  = ~0U;
    unsigned interruption_index = ~0U;
    unsigned timeout_index      = ~0U;

    if (getCurrentThreadData()) {
        interruption_index      = handle_count;
        handles[handle_count++] = getCurrentThreadData()->hEventHandle;
    }
    HANDLE timer_handle;
    // timespec_tt tmpTime;

    {
        getClockMonotonic(&now);
        int64_t time_left_msec = (timeoutNs - timespec_toNsec(&now)) / 1000000;

        // timespec_sub(&timeout,&now, &tmpTime);
        // int64_t time_left_msec = timespec_toMsec(&tmpTime);
        timer_handle = CreateWaitableTimer(NULL, false, NULL);
        if (timer_handle != 0) {
            ULONG tolerable = 32;
            if (time_left_msec / 20 > tolerable) tolerable = (ULONG)(time_left_msec / 20);
            LARGE_INTEGER due_time = {{0, 0}};
            if (time_left_msec > 0) {
                due_time.QuadPart = -(time_left_msec * 10000);
            }
            bool const set_time_succeeded =
                getSetWaitableTimerEx()(timer_handle, &due_time, 0, 0, 0, NULL, tolerable) != 0;
            if (set_time_succeeded) {
                timeout_index           = handle_count;
                handles[handle_count++] = timer_handle;
            }
        }
    }

    bool const using_timer    = timeout_index != ~0u;
    int64_t    time_left_msec = INFINITE;
    if (!using_timer) {
        getClockMonotonic(&now);
        // timespec_sub(&timeout,&now,&tmpTime);
        // time_left_msec = timespec_toMsec(&tmpTime);

        time_left_msec = (timeoutNs - timespec_toNsec(&now)) / 1000000;
        if (time_left_msec < 0) {
            time_left_msec = 0;
        }
    }

    do {
        if (handle_count) {
            unsigned long const notified_index =
                WaitForMultipleObjectsEx(handle_count, handles, false, (DWORD)(time_left_msec), 0);
            if (notified_index < handle_count) {
                if (notified_index == wait_handle_index) {
                    return true;
                }
                else if (notified_index == interruption_index) {
                    ResetEvent(getCurrentThreadData()->hEventHandle);
                    return false;
                }
                else if (notified_index == timeout_index) {
                    return false;
                }
            }
        }
        else {
            Sleep((unsigned long)(time_left_msec));
        }

        if (!using_timer) {
            // getClockMonotonic(&now);
            // timespec_sub(&timeout, &now, &tmpTime);
            // time_left_msec = timespec_toMsec(&tmpTime);
            time_left_msec -= timespec_toMsec(&now);
        }
    } while (time_left_msec == INFINITE || time_left_msec > 0);
    return false;
}

uint64_t getThreadClock()
{
    FILETIME creation;
    FILETIME exit;
    FILETIME kernelTime;
    FILETIME userTime;
    if (GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernelTime, &userTime)) {
        uint64_t uiUser =
            ((((uint64_t)(userTime.dwHighDateTime) << 32) | (uint64_t)userTime.dwLowDateTime) *
             100LL);
        uint64_t uiKernel =
            ((((uint64_t)(kernelTime.dwHighDateTime) << 32) | (uint64_t)kernelTime.dwLowDateTime) *
             100LL);
        return uiUser + uiKernel;
    }
    return 0;
}
