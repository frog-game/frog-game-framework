#include "spinLock.h"

#include <stdatomic.h>

#include "thread_t.h"
#include "time_t.h"
#include "utility_t.h"

struct spinLock_s
{
    atomic_flag flag;
};

void spinLock_init(spinLock_tt* lock)
{
    *lock = mem_malloc(sizeof(struct spinLock_s));
    atomic_flag_clear(&((*lock)->flag));
}

void spinLock_destroy(spinLock_tt* lock)
{
    mem_free(*lock);
    *lock = NULL;
}

static const uint32_t s_uiMaxActiveSpin = 2048;

void spinLock_lock(spinLock_tt* lock)
{
    uint32_t spinCount = 0;
    if (atomic_flag_test_and_set_explicit(&((*lock)->flag), memory_order_acquire)) {
        do {
            if (spinCount < s_uiMaxActiveSpin) {
                ++spinCount;
                thread_pause();
            }
            else {
                threadYield();
            }
        } while (atomic_flag_test_and_set_explicit(&((*lock)->flag), memory_order_relaxed));
    }
}

void spinLock_lock2(spinLock_tt* lock)
{
    while (atomic_flag_test_and_set_explicit(&((*lock)->flag), memory_order_acquire)) {
        threadYield();
    }
}

void spinLock_lock3(spinLock_tt* lock)
{
    if (atomic_flag_test_and_set_explicit(&((*lock)->flag), memory_order_acquire)) {
        do {
            threadYield();
        } while (atomic_flag_test_and_set_explicit(&((*lock)->flag), memory_order_relaxed));
    }
}

void spinLock_unlock(spinLock_tt* lock)
{
    atomic_flag_clear_explicit(&((*lock)->flag), memory_order_release);
}
