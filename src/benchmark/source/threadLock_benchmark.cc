#include "benchmark/benchmark.h"

#include <stdio.h>
#include <stdlib.h>

#include <atomic>
#include <inttypes.h>

#include <assert.h>

extern "C" {
    #include "time_t.h"
    #include "thread_t.h"
    #include "time_t.h"
    #include "byteQueue_t.h"
    #include "queue_t.h"
    #include "spin_lock/spinLock.h"
    #include "spin_lock/clhLock.h"
    #include "spin_lock/mscLock.h"
    #include "spin_lock/rwSpinLock.h"
}

#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", off)
inline void doNotOptimizeDependencySink(const void*) {}
#pragma optimize("", on)

template <class T>
void doNotOptimizeAway(const T& datum) {
    doNotOptimizeDependencySink(&datum);
}

#else
template <typename T>
void doNotOptimizeAway(const T& datum)
{
    asm volatile("" ::"r"(datum));
}
#endif

static void burn(size_t n) {
  for (size_t i = 0; i < n; ++i) {
    doNotOptimizeAway(i);
  }
}


void BM_empty(benchmark::State& state) {
  for (auto _ : state) {
    burn(state.iterations());
  }
}

//BENCHMARK(BM_empty)->Iterations(500000);
// BENCHMARK(BM_empty)->ThreadPerCpu()->Iterations(500000);
// BENCHMARK(BM_empty)->Threads(8)->Iterations(500000);
// BENCHMARK(BM_empty)->ThreadRange(1, 32)->Iterations(500000);
// BENCHMARK(BM_empty)->Threads(1)->Iterations(500000);

//BENCHMARK(BM_empty)->Arg(8)->Arg(512)->Arg(8192)->Threads(1);
//BENCHMARK(BM_empty)->Args({8,16})->Threads(1)->Iterations(100);
// BENCHMARK(BM_empty)->Arg(8)->Arg(512)->Arg(8192)->Threads(5);

typedef struct nodeValue_s 
{
	void*       node[2];
    uint32_t    uiIndex;
    int32_t     iThreadIndex;
} nodeValue_tt;

class threadLockData
{
public:
  	threadLockData();
	~threadLockData();

	void lock(int32_t iSelectLock);

	void unlock(int32_t iSelectLock);

    spinLock_tt         spinLock;
    clhLock_tt          clhLock;
    mscLock_tt          mscLock;
    mutex_tt            mutex;
    QUEUE               queuePending;
};

threadLockData::threadLockData()
{
	mutex_init(&mutex);
    spinLock_init(&spinLock);
    clhLock_init(&clhLock);
    mscLock_init(&mscLock);
    QUEUE_INIT(&queuePending);
}


threadLockData::~threadLockData()
{
	mutex_destroy(&mutex);
    spinLock_destroy(&spinLock);
    clhLock_destroy(&clhLock);
    mscLock_destroy(&mscLock);
}

void threadLockData::lock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    mutex_lock(&mutex);
        }
        break;
    case 1:
        {
            spinLock_lock(&spinLock);
        }
        break;
    case 2:
        {
            spinLock_lock2(&spinLock);
        }
        break;
    case 3:
        {
            spinLock_lock3(&spinLock);
        }
        break;
    case 4:
        {
            clhLock_lock(&clhLock);
        }
        break;
    case 5:
        {
            mscLock_lock(&mscLock);
        }
        break;
    }
}

void threadLockData::unlock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    mutex_unlock(&mutex);
        }
        break;
    case 1:
    case 2:
    case 3:
        {
            spinLock_unlock(&spinLock);
        }
        break;
    case 4:
        {
            clhLock_unlock(&clhLock);
        }
        break;
    case 5:
        {
            mscLock_unlock(&mscLock);
        }
        break;
    }
}

threadLockData testLockData;

void BM_LockPushPopFunctors(benchmark::State& state)
{
    for (auto _ : state) 
    {
        if (state.thread_index == 0)
        {
            uint32_t uiTestCount[256] = { 0 };
            int32_t iCount = 0;

            QUEUE* pNode = NULL;
            nodeValue_tt* pValue = NULL;
            QUEUE  queuePendingDelete;
            QUEUE_INIT(&queuePendingDelete);

            for (;;)
            {
                burn(100);
                testLockData.lock(state.range(0));
                if (!QUEUE_EMPTY(&testLockData.queuePending))
                {
                    pNode = QUEUE_HEAD(&testLockData.queuePending);
                    QUEUE_REMOVE(pNode);
                    pValue = container_of(pNode, nodeValue_tt, node);
                }
                testLockData.unlock(state.range(0));
                if (pValue)
                {
                    ++iCount;
                    ++uiTestCount[pValue->iThreadIndex];
                    assert(pValue->uiIndex == uiTestCount[pValue->iThreadIndex]);
                    QUEUE_INSERT_TAIL(&queuePendingDelete, &pValue->node);
                    pValue = NULL;
                }

                if (iCount == state.range(1) * (state.threads - 1))
                {
                    break;
                }
            }

            while (!QUEUE_EMPTY(&queuePendingDelete))
            {
                pNode = QUEUE_HEAD(&queuePendingDelete);
                pValue = container_of(pNode, nodeValue_tt, node);
                QUEUE_REMOVE(pNode);
                free(pValue);
            }
        }
        else
        {
            for (int i = 0; i < state.range(1); ++i)
            {
                burn(1000);
                nodeValue_tt* pValue = (nodeValue_tt*)malloc(sizeof(nodeValue_tt));
                pValue->uiIndex = i + 1;
                pValue->iThreadIndex = state.thread_index - 1;
                testLockData.lock(state.range(0));
                QUEUE_INSERT_TAIL(&testLockData.queuePending, &pValue->node);
                testLockData.unlock(state.range(0));

            }
        }
    }
}


void BM_LockPushMoveFunctors(benchmark::State& state)
{
    for (auto _ : state) 
    {
        if (state.thread_index == 0)
        {
            uint32_t uiTestCount[256] = { 0 };
            int32_t iCount = 0;

            QUEUE queuePending;
            QUEUE_INIT(&queuePending);
            QUEUE* pNode = NULL;
            nodeValue_tt* pValue = NULL; 
            QUEUE  queuePendingDelete;
            QUEUE_INIT(&queuePendingDelete);

            for (;;)
            {
                burn(100);
                testLockData.lock(state.range(0));
		        QUEUE_MOVE(&testLockData.queuePending, &queuePending);
                testLockData.unlock(state.range(0));

                if(!QUEUE_EMPTY(&queuePending))
                {
                    do 
                    {
                        pNode = QUEUE_HEAD(&queuePending);
                        QUEUE_REMOVE(pNode);
                        pValue = container_of(pNode, nodeValue_tt, node);
                        ++iCount;
                        ++uiTestCount[pValue->iThreadIndex];
                        assert(pValue->uiIndex == uiTestCount[pValue->iThreadIndex]);
                        QUEUE_INSERT_TAIL(&queuePendingDelete, &pValue->node);
                        pValue = NULL;
                    }
                    while(!QUEUE_EMPTY(&queuePending));
                }

                if (iCount == state.range(1) * (state.threads - 1))
                {
                    break;
                }
            }

            while (!QUEUE_EMPTY(&queuePendingDelete))
            {
                pNode = QUEUE_HEAD(&queuePendingDelete);
                pValue = container_of(pNode, nodeValue_tt, node);
                QUEUE_REMOVE(pNode);
                free(pValue);
            }
        }
        else
        {
            for (int i = 0; i < state.range(1); ++i)
            {
                burn(1000);
                nodeValue_tt* pValue = (nodeValue_tt*)malloc(sizeof(nodeValue_tt));
                pValue->uiIndex = i + 1;
                pValue->iThreadIndex = state.thread_index - 1;
                testLockData.lock(state.range(0));
                QUEUE_INSERT_TAIL(&testLockData.queuePending, &pValue->node);
                testLockData.unlock(state.range(0));

            }
        }
    }
}
BENCHMARK(BM_LockPushPopFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(2);
BENCHMARK(BM_LockPushPopFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(5);
BENCHMARK(BM_LockPushPopFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(10);
BENCHMARK(BM_LockPushPopFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(20);
BENCHMARK(BM_LockPushPopFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->ThreadPerCpu();

BENCHMARK(BM_LockPushMoveFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(2);
BENCHMARK(BM_LockPushMoveFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(5);
BENCHMARK(BM_LockPushMoveFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(10);
BENCHMARK(BM_LockPushMoveFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->Threads(20);
BENCHMARK(BM_LockPushMoveFunctors)->Args({ 0, 500000 })->Args({ 1, 500000 })->Args({ 2, 500000 })->Args({ 3, 500000 })->Args({ 4, 500000 })->Args({ 5, 500000 })->ThreadPerCpu();

// BENCHMARK(BM_empty)->ThreadPerCpu()->Iterations(500000);
// BENCHMARK(BM_empty)->Threads(8)->Iterations(500000);
// BENCHMARK(BM_empty)->ThreadRange(1, 32)->Iterations(500000);
// BENCHMARK(BM_empty)->Threads(1)->Iterations(500000);


class threadRWLockData
{
public:
    threadRWLockData();
    ~threadRWLockData();

    void rdlock(int32_t iSelectLock);

    void rdunlock(int32_t iSelectLock);

    void wrlock(int32_t iSelectLock);

    void wrunlock(int32_t iSelectLock);

public:
    int32_t             iWIndex;
    int32_t             iIndex;

private:
    rwlock_tt           rwlock;
    rwSpinLock_tt       spinlock;
};

threadRWLockData::threadRWLockData():iWIndex(0), iIndex(0)
{
	rwlock_init(&rwlock);
    rwSpinLock_init(&spinlock);
}

threadRWLockData::~threadRWLockData()
{
	rwlock_destroy(&rwlock);
}

void threadRWLockData::rdlock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    rwlock_rdlock(&rwlock);
        }
        break;
    case 1:
        {
            rwSpinLock_rdlock(&spinlock);
        }
        break;
    }
}

void threadRWLockData::rdunlock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    rwlock_rdunlock(&rwlock);
        }
        break;
    case 1:
        {
            rwSpinLock_rdunlock(&spinlock);
        }
        break;
    }
}


void threadRWLockData::wrlock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    rwlock_wrlock(&rwlock);
        }
        break;
    case 1:
        {
            rwSpinLock_wrlock(&spinlock);
        }
        break;
    }
}

void threadRWLockData::wrunlock(int32_t iSelectLock)
{
    switch (iSelectLock)
    {
    case 0:
        {
		    rwlock_wrunlock(&rwlock);
        }
        break;
    case 1:
        {
            rwSpinLock_wrunlock(&spinlock);
        }
        break;
    }
}

threadRWLockData testRWLockData;

void BM_RWLockFunctors(benchmark::State& state)
{
    if (state.thread_index == 0)
    {
        testRWLockData.iWIndex = 0;
        testRWLockData.iIndex = 0;
    }

    for (auto _ : state) 
    {
        burn(1000);
        if(state.iterations() %state.range(1) == 0)
        {
            testRWLockData.wrlock(state.range(0));
            ++testRWLockData.iWIndex;
            testRWLockData.iIndex = testRWLockData.iWIndex;
            assert(testRWLockData.iIndex== testRWLockData.iWIndex);
            testRWLockData.wrunlock(state.range(0));
        }
        else
        {
            testRWLockData.rdlock(state.range(0));
            assert(testRWLockData.iIndex== testRWLockData.iWIndex);
            testRWLockData.rdunlock(state.range(0));
        }
    }
}


BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->Threads(1);
BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->Threads(2);
BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->Threads(5);
BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->Threads(10);
BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->Threads(20);
BENCHMARK(BM_RWLockFunctors)->Args({ 0, 10 })->Args({ 1, 10 })->Args({ 0, 100 })->Args({ 1, 100 })->Iterations(500000)->ThreadPerCpu();
