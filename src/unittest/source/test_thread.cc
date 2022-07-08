#include "gtest/gtest.h"

#include <stdio.h>
#include <stdlib.h>

#include <atomic>
#include <inttypes.h>

extern "C" {
#include "thread_t.h"
#include "time_t.h"
#include "byteQueue_t.h"
}


static std::atomic_int g_iThreadCount = ATOMIC_VAR_INIT(0);
static std::atomic_int g_iTestCount = ATOMIC_VAR_INIT(0);
static once_flag_tt once = ONCE_FLAG_INIT;

static void onceFunc()
{
	std::atomic_fetch_add(&g_iTestCount,1);
}

static void threadRunFunc(void* pArg)
{
	std::atomic_fetch_add(&g_iThreadCount,1);
	callOnce(&once, onceFunc);
}

TEST(thread, test_0)
{
	std::atomic_store(&g_iThreadCount,0);
	callOnce(&once, onceFunc);
	ASSERT_EQ(std::atomic_load(&g_iTestCount),1);
	callOnce(&once, onceFunc);
	ASSERT_EQ(std::atomic_load(&g_iTestCount),1);

	thread_tt thread[5];
	for(int32_t i = 0; i< 5; ++i)
	{
		ASSERT_EQ(thread_start(&thread[i],threadRunFunc,NULL),eThreadSuccess);
	}

	for(int32_t i = 0; i< 5; ++i)
	{
		thread_join(thread[i]);
	}
	ASSERT_EQ(std::atomic_load(&g_iTestCount),1);
	ASSERT_EQ(std::atomic_load(&g_iThreadCount),5);
}

	

static void threadFuncTest(void* pArg)
{
	std::atomic_fetch_add(&g_iThreadCount,1);
	thread_tt self = threadSelf();
	ASSERT_TRUE(threadEqual(self,threadSelf()));
}

TEST(thread, test_1)
{
	thread_tt thread[5];
	std::atomic_store(&g_iThreadCount,0);
	for(int32_t i = 0; i< 5; ++i)
	{
		ASSERT_EQ(thread_start(&thread[i],threadFuncTest,NULL),eThreadSuccess);
	}
	for(int32_t i = 0; i< 5; ++i)
	{
		thread_join(thread[i]);
	}
}

volatile int64_t iWaitTimer[5];

volatile bool bExit[5] = {true,true,true,true,true};

static void threadFuncTest2(void* pArg)
{
	ASSERT_NE(pArg,nullptr);
	int32_t i = *((int32_t*)pArg);
	timespec_tt time;
	timespec_tt time2;
	getClockMonotonic(&time);
	threadYield();
	getClockMonotonic(&time2);
	iWaitTimer[i] = timespec_subToNs(&time2,&time);
	threadExit();
	bExit[i] = false;
}

TEST(thread, test_2)
{
	thread_tt thread[5];
	int32_t iIndex[5] = {0,1,2,3,4};
	for(int32_t i = 0; i< 5; ++i)
	{
		ASSERT_EQ(thread_start(&thread[i],threadFuncTest2,&iIndex[i]),eThreadSuccess);
	}
	for(int32_t i = 0; i< 5; ++i)
	{
		thread_join(thread[i]);
		printf("sleep:%" PRId64"ns\n",iWaitTimer[i]);
		ASSERT_TRUE(bExit[i]);
	}
}

void cleanup_func(void* pArg)
{
	free(pArg);
}

static void threadFuncTest3(void* pArg)
{
	int32_t* pValue = (int32_t*)malloc(sizeof(int32_t));
	printf("threadFuncTest3 1\n");
	void* p = getTlsValue("testTlsKey");
	printf("threadFuncTest3 2\n");
	ASSERT_EQ(p,nullptr);
	printf("threadFuncTest3 3\n");
	*pValue = 38;
	setTlsValue("testTlsKey",cleanup_func,pValue,true);
	printf("threadFuncTest3 4\n");
	void* p2 = getTlsValue("testTlsKey");
	printf("threadFuncTest3 5\n");
	if( p2 == NULL)
	{
		printf("threadFuncTest3 5  null\n");
	}
	else
	{
		printf("threadFuncTest3 5 non null\n");
		
	}
	
	ASSERT_NE(p2,nullptr);
	printf("threadFuncTest3 6\n");
	ASSERT_EQ(*((int32_t*)p2),38);
	printf("threadFuncTest3 7\n");
}

TEST(thread, test_3)
{
	int32_t iValue = 8;

	setTlsValue("testTlsKey",NULL,&iValue,false);
	thread_tt thread;
	ASSERT_EQ(thread_start(&thread,threadFuncTest3,NULL),eThreadSuccess);
	thread_join(thread);
	void* p = getTlsValue("testTlsKey");
	ASSERT_NE(p,nullptr);
	ASSERT_EQ(p,&iValue);
	ASSERT_EQ(*((int32_t*)p),8);
}

TEST(thread, test_4)
{
	mutex_tt mutex;
	ASSERT_TRUE(mutex_init(&mutex));
	mutex_lock(&mutex);
	ASSERT_TRUE(mutex_trylock(&mutex));
	mutex_unlock(&mutex);
	mutex_unlock(&mutex);
	mutex_destroy(&mutex);
}

volatile int32_t iCount = 0;
static mutex_tt s_mutex;

void threadFuncTest5(void* pArg)
{
	mutex_lock(&s_mutex);
	++iCount;
	mutex_unlock(&s_mutex);
}

TEST(thread, test_5)
{
	ASSERT_TRUE(mutex_init(&s_mutex));
	thread_tt thread[5];
	for(int32_t i = 0; i< 5; ++i)
	{
		ASSERT_EQ(thread_start(&thread[i],threadFuncTest5,NULL),eThreadSuccess);
	}
	for(int32_t i = 0; i< 5; ++i)
	{
		thread_join(thread[i]);
	}
	mutex_destroy(&s_mutex);
	ASSERT_EQ(iCount,5);
}


TEST(thread, test_6)
{
	rwlock_tt rwlock;
	ASSERT_TRUE(rwlock_init(&rwlock));
	rwlock_rdlock(&rwlock);
	rwlock_rdunlock(&rwlock);
	rwlock_wrlock(&rwlock);
	rwlock_wrunlock(&rwlock);
	rwlock_destroy(&rwlock);
}


static cond_tt condvar;
static mutex_tt mutex;
static rwlock_tt rwlock;
static int32_t step;

static void synchronize_nowait(void) {
  step += 1;
  cond_signal(&condvar);
}

static void synchronize(void)
{
	int32_t current;
	synchronize_nowait();
	for (current = step; current == step; cond_wait(&condvar, &mutex));
	
	ASSERT_EQ(step,current + 1);
	
}


static void thread_rwlock_trylock_peer(void* pArg) 
{
	mutex_lock(&mutex);
	ASSERT_FALSE(rwlock_tryrdlock(&rwlock));
	ASSERT_FALSE(rwlock_trywrlock(&rwlock));
	synchronize();

	ASSERT_TRUE(rwlock_tryrdlock(&rwlock));
	rwlock_rdunlock(&rwlock);
	
	ASSERT_FALSE(rwlock_trywrlock(&rwlock));
	synchronize();

	ASSERT_TRUE(rwlock_trywrlock(&rwlock));
	synchronize();

	rwlock_wrunlock(&rwlock);
	ASSERT_TRUE(rwlock_tryrdlock(&rwlock));
	synchronize();

	rwlock_rdunlock(&rwlock);
	synchronize_nowait(); 
	mutex_unlock(&mutex);
}


TEST(thread, test_7)
{
	thread_tt thread;

	ASSERT_TRUE(cond_init(&condvar));
	ASSERT_TRUE(mutex_init(&mutex));
	ASSERT_TRUE(rwlock_init(&rwlock));

	mutex_lock(&mutex);
	
	ASSERT_EQ(thread_start(&thread,thread_rwlock_trylock_peer,NULL),eThreadSuccess);

	ASSERT_TRUE(rwlock_trywrlock(&rwlock));
	synchronize(); 

	rwlock_wrunlock(&rwlock);
	ASSERT_TRUE(rwlock_tryrdlock(&rwlock));
	synchronize();

	rwlock_rdunlock(&rwlock);
	synchronize();

	ASSERT_FALSE(rwlock_tryrdlock(&rwlock));
	ASSERT_FALSE(rwlock_trywrlock(&rwlock));
	synchronize();

	ASSERT_TRUE(rwlock_tryrdlock(&rwlock));
	rwlock_rdunlock(&rwlock);
	ASSERT_FALSE(rwlock_trywrlock(&rwlock));
	synchronize();

	thread_join(thread);
	rwlock_destroy(&rwlock);
	mutex_unlock(&mutex);
	mutex_destroy(&mutex);
	cond_destroy(&condvar);
}


typedef struct {
	mutex_tt mutex;
	sem_tt sem;
	timespec_tt delay;
	volatile int32_t posted;
} worker_config;


static void worker(void* arg) 
{
	worker_config* c = (worker_config*)arg;

	if(!timespec_zero(&c->delay))
	{
		sleep_for(&c->delay);
	}

	mutex_lock(&c->mutex);
	ASSERT_EQ(c->posted,0);
	sema_post(&c->sem);
	c->posted = 1;
	mutex_unlock(&c->mutex);
}

TEST(thread, test_8)
{
	thread_tt thread;
	worker_config wc;
	memset(&wc, 0, sizeof(wc));

	ASSERT_TRUE(sema_init(&wc.sem, 0));
	ASSERT_TRUE(mutex_init(&wc.mutex));
	ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

	timespec_tt timeSleep = {1,0};
	sleep_for(&timeSleep);
	mutex_lock(&wc.mutex);
	ASSERT_EQ(wc.posted ,1);
	sema_wait(&wc.sem); 
	mutex_unlock(&wc.mutex); 
	thread_join(thread);

	mutex_destroy(&wc.mutex);
	sema_destroy(&wc.sem);
}


TEST(thread, test_9)
{
	thread_tt thread;
	worker_config wc;

	memset(&wc, 0, sizeof(wc));
	wc.delay.iSec = 1;
	wc.delay.iNsec = 0;
	ASSERT_TRUE(sema_init(&wc.sem, 0));
	ASSERT_TRUE(mutex_init(&wc.mutex));
	ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

	sema_wait(&wc.sem);
	thread_join(thread);
	mutex_destroy(&wc.mutex);
	sema_destroy(&wc.sem);
}

TEST(thread, test_10)
{
	sem_tt sem;
	ASSERT_TRUE(sema_init(&sem, 3));
	sema_wait(&sem); /* should not block */
	sema_wait(&sem); /* should not block */
	ASSERT_TRUE(sema_trywait(&sem));
	ASSERT_FALSE(sema_trywait(&sem));
	sema_post(&sem);
	ASSERT_TRUE(sema_trywait(&sem));
	ASSERT_FALSE(sema_trywait(&sem));
	sema_destroy(&sem);
}


static void threadTimerFunc(void* pArg)
{
	uint64_t uiThreadTime1 = getThreadClock();
	byteQueue_tt byteQueue;
	byteQueue_init(&byteQueue,0);
	for(int32_t i = 0; i< 10000; ++i)
	{
		size_t n = strlen("test");
		byteQueue_writeBytes(&byteQueue,"test",n);

		timespec_tt timeSleep;
		timeSleep.iSec = 0;
		timeSleep.iNsec = 1000000;

		if (i % 10 == 0)
		{
			sleep_for(&timeSleep);
		}
	}
	byteQueue_clear(&byteQueue);
	uint64_t uiThreadTime2 = getThreadClock();
	uint64_t uiThreadTime3 = uiThreadTime2 -uiThreadTime1;
	printf("thread cur:%" PRIu64" ns last:%" PRIu64" ns sleep:%" PRIu64" ns\n",uiThreadTime1,uiThreadTime2,uiThreadTime3);
}

TEST(thread, test_11)
{
	timespec_tt time1;
	timespec_tt time2;
	timespec_tt time3;
	getClockMonotonic(&time1);

	thread_tt thread;
	ASSERT_EQ(thread_start(&thread,threadTimerFunc,NULL),eThreadSuccess);
	thread_join(thread);
	
	getClockMonotonic(&time2);
	timespec_sub(&time2,&time1,&time3)

	uint64_t uiTimer1Ns = timespec_toNsec(&time1);
	uint64_t uiTimer2Ns = timespec_toNsec(&time2);
	uint64_t uiTimer3Ns = timespec_toNsec(&time3);
	printf("cur:%" PRIu64" ns last:%" PRIu64" ns sleep:%" PRIu64" ns\n",uiTimer1Ns,uiTimer2Ns,uiTimer3Ns);
}