#pragma once

#include <stdatomic.h>

#include "thread_t.h"

typedef struct spinLock_s
{
    atomic_flag flag;
} spinLock_tt;

static inline void spinLock_init(spinLock_tt* self)
{
    atomic_flag_clear(&self->flag);
}

static inline void spinLock_lock(spinLock_tt* self)
{
    uint32_t uiSpinCount = 0;
    if (atomic_flag_test_and_set_explicit(&self->flag, memory_order_acquire)) {
        do {
            if (uiSpinCount++ < 2048) {
                thread_pause();
            }
            else {
                threadYield();
            }
        } while (atomic_flag_test_and_set_explicit(&self->flag, memory_order_relaxed));
    }
}

static inline void spinLock_unlock(spinLock_tt* self)
{
    atomic_flag_clear_explicit(&self->flag, memory_order_release);
}

static inline bool spinLock_trylock(spinLock_tt* self)
{
    return !atomic_flag_test_and_set_explicit(&self->flag, memory_order_acquire);
}