#include "gtest/gtest.h"

#include <stdio.h>
#include <stdlib.h>

#include <atomic>

extern "C" {
#include "thread_t.h"
#include "time_t.h"
}


struct worker_config;

typedef void (*signal_func)(struct worker_config*, int32_t*);
typedef int32_t (*wait_func)(struct worker_config*, const int32_t*);

typedef struct worker_config {
	sem_tt sem_waiting; /* post before waiting. */
	sem_tt sem_signaled; /* post after signaling. */
	mutex_tt mutex;
	cond_tt cond;
	int32_t use_broadcast;
	int32_t posted_1;
	int32_t posted_2;
	signal_func signal_cond;
	wait_func wait_cond;
} worker_config;

void worker_config_init(worker_config* wc,
                        int32_t use_broadcast,
                        signal_func signal_f,
                        wait_func wait_f) {
  	memset(wc, 0, sizeof(*wc));

	wc->signal_cond = signal_f;
	wc->wait_cond = wait_f;
	wc->use_broadcast = use_broadcast;

	ASSERT_TRUE(sema_init(&wc->sem_waiting, 0));
	ASSERT_TRUE(sema_init(&wc->sem_signaled, 0));
	ASSERT_TRUE(cond_init(&wc->cond));
	ASSERT_TRUE(mutex_init(&wc->mutex));
}

void worker_config_destroy(worker_config* wc) 
{
	mutex_destroy(&wc->mutex);
	cond_destroy(&wc->cond);
	sema_destroy(&wc->sem_signaled);
	sema_destroy(&wc->sem_waiting);
}


static void worker(void* arg) {
  worker_config* c = (worker_config* )arg;
  c->signal_cond(c, &c->posted_1);
  c->wait_cond(c, &c->posted_2);
}


static void condvar_signal(worker_config* c, int32_t* pFlag) 
{
	sema_wait(&c->sem_waiting);

	mutex_lock(&c->mutex);

    EXPECT_EQ(*pFlag ,0);
	*pFlag = 1;

	if (c->use_broadcast)
		cond_broadcast(&c->cond);
	else
		cond_signal(&c->cond);

	mutex_unlock(&c->mutex);

	sema_post(&c->sem_signaled);
}

static int32_t condvar_wait(worker_config* c, const int32_t* pFlag) 
{
	mutex_lock(&c->mutex);

	sema_post(&c->sem_waiting);

	do {
		cond_wait(&c->cond, &c->mutex);
	} while (*pFlag == 0);
    
    EXPECT_EQ(*pFlag,1);

	mutex_unlock(&c->mutex);

	sema_wait(&c->sem_signaled);
  	return 0;
}

TEST(thread2, test_0)
{
	worker_config wc;
	thread_tt thread;

	worker_config_init(&wc, 0, condvar_signal, condvar_wait);
    ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

    ASSERT_EQ(wc.wait_cond(&wc, &wc.posted_1),0);
	wc.signal_cond(&wc, &wc.posted_2);
    thread_join(thread);
	worker_config_destroy(&wc);

}

TEST(thread2, test_1)
{
	worker_config wc;
	thread_tt thread;

	/* Helper to signal-then-wait. */
	worker_config_init(&wc, 1, condvar_signal, condvar_wait);
    ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

    ASSERT_EQ(wc.wait_cond(&wc, &wc.posted_1),0);
	wc.signal_cond(&wc, &wc.posted_2);

    thread_join(thread);
	worker_config_destroy(&wc);
}


static int32_t condvar_timedwait(worker_config* c, const int32_t* pFlag) 
{
	int32_t r = 0;

	mutex_lock(&c->mutex);

	sema_post(&c->sem_waiting);

	do {
		r = cond_timedwait(&c->cond, &c->mutex, (uint64_t)(1 * 1e9)); /* 1 s */
		EXPECT_EQ(r,eThreadSuccess);
	} while (*pFlag == 0);

	EXPECT_EQ( *pFlag ,1 );
	mutex_unlock(&c->mutex);

	sema_wait(&c->sem_signaled);
  	return r;
}

TEST(thread2, test_2)
{
	worker_config wc;
	thread_tt thread;

	/* Helper to signal-then-wait. */
	worker_config_init(&wc, 0, condvar_signal, condvar_timedwait);
    ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

	wc.wait_cond(&wc, &wc.posted_1);
	wc.signal_cond(&wc, &wc.posted_2);

	thread_join(thread);
	worker_config_destroy(&wc);
}

TEST(thread2, test_3)
{
	worker_config wc;
	thread_tt thread;

	/* Helper to signal-then-wait. */
	worker_config_init(&wc, 1, condvar_signal, condvar_timedwait);
    ASSERT_EQ(thread_start(&thread,worker,&wc),eThreadSuccess);

	wc.wait_cond(&wc, &wc.posted_1);
	wc.signal_cond(&wc, &wc.posted_2);

	thread_join(thread);
	worker_config_destroy(&wc);
}

TEST(thread2, test_4)
{
	worker_config wc;
	int32_t r;
	/* ns */
	int64_t elapsed;
	uint64_t timeout;

	timeout = 100 * 1000 * 1000; /* 100 ms in ns */

	/* Mostly irrelevant. We need cond and mutex initialized. */
	worker_config_init(&wc, 0, NULL, NULL);

	mutex_lock(&wc.mutex);

    timespec_tt before;
    timespec_tt after;
    getClockMonotonic(&before);

	r = cond_timedwait(&wc.cond, &wc.mutex, timeout);
    getClockMonotonic(&after);

	mutex_unlock(&wc.mutex);

	ASSERT_EQ(r ,eThreadTimedout);

	elapsed = timespec_subToNs(&after,&before);
	ASSERT_TRUE(0.75 * timeout <= elapsed); /* 1.0 too large for Windows. */
	ASSERT_TRUE(elapsed <= 5.0 * timeout); /* MacOS has reported failures up to 1.75. */
	worker_config_destroy(&wc);

}


