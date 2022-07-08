#include "rwSpinLock.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "thread_t.h"
#include "utility_t.h"

enum
{
    RW_WRITER   = 1,
    RW_UPGRADED = 2,
    RW_READER   = 4
};

struct rwSpinLock_s
{
    atomic_int iBits;
};

void rwSpinLock_init(rwSpinLock_tt* self)
{
    *self = mem_malloc(sizeof(struct rwSpinLock_s));
    atomic_init(&((*self)->iBits), 0);
}

void rwSpinLock_destroy(rwSpinLock_tt* self)
{
    mem_free(*self);
    *self = NULL;
}

static inline bool tryrdlock(rwSpinLock_tt* self)
{
    int32_t v = atomic_fetch_add_explicit(&((*self)->iBits), RW_READER, memory_order_acquire);
    if (_UnLikely(v & (RW_WRITER | RW_UPGRADED))) {
        atomic_fetch_add_explicit(&((*self)->iBits), -RW_READER, memory_order_release);
        return false;
    }
    return true;
}

static inline bool tryuplock(rwSpinLock_tt* self)
{
    int32_t v = atomic_fetch_or_explicit(&((*self)->iBits), RW_UPGRADED, memory_order_acquire);
    return ((v & (RW_UPGRADED | RW_WRITER)) == 0);
}

static inline bool trywrlock(rwSpinLock_tt* self)
{
    int32_t iExpect = 0;
    return atomic_compare_exchange_strong_explicit(
        &((*self)->iBits), &iExpect, RW_WRITER, memory_order_acq_rel, memory_order_relaxed);
}

static inline bool tryupunlock_wrlock(rwSpinLock_tt* self)
{
    int32_t iExpect = RW_UPGRADED;
    return atomic_compare_exchange_strong_explicit(
        &((*self)->iBits), &iExpect, RW_WRITER, memory_order_acq_rel, memory_order_relaxed);
}

void rwSpinLock_rdlock(rwSpinLock_tt* self)
{
    uint32_t uiSpinCount = 0;
    while (!_Likely(tryrdlock(self))) {
        if (++uiSpinCount > 1024) {
            threadYield();
        }
    }
}

bool rwSpinLock_tryrdlock(rwSpinLock_tt* self)
{
    return tryrdlock(self);
}

void rwSpinLock_rdunlock(rwSpinLock_tt* self)
{
    atomic_fetch_add_explicit(&((*self)->iBits), -RW_READER, memory_order_release);
}

void rwSpinLock_wrlock(rwSpinLock_tt* self)
{
    uint32_t uiSpinCount = 0;
    while (!_Likely(trywrlock(self))) {
        if (++uiSpinCount > 1024) {
            threadYield();
        }
    }
}

bool rwSpinLock_trywrlock(rwSpinLock_tt* self)
{
    return trywrlock(self);
}

void rwSpinLock_wrunlock(rwSpinLock_tt* self)
{
    atomic_fetch_and_explicit(&((*self)->iBits), ~(RW_WRITER | RW_UPGRADED), memory_order_release);
}

void rwSpinLock_wrunlock_rdlock(rwSpinLock_tt* self)
{
    atomic_fetch_add_explicit(&((*self)->iBits), RW_READER, memory_order_acquire);
    atomic_fetch_and_explicit(&((*self)->iBits), ~(RW_WRITER | RW_UPGRADED), memory_order_release);
}

void rwSpinLock_upLock(rwSpinLock_tt* self)
{
    uint32_t uiSpinCount = 0;
    while (!tryuplock(self)) {
        if (++uiSpinCount > 1024) {
            threadYield();
        }
    }
}

void rwSpinLock_upunLock(rwSpinLock_tt* self)
{
    atomic_fetch_add_explicit(&((*self)->iBits), -RW_UPGRADED, memory_order_acq_rel);
}

bool rwSpinLock_tryuplock(rwSpinLock_tt* self)
{
    return tryuplock(self);
}

void rwSpinLock_upunlock_wrlock(rwSpinLock_tt* self)
{
    uint32_t uiSpinCount = 0;
    while (!tryupunlock_wrlock(self)) {
        if (++uiSpinCount > 1024) {
            threadYield();
        }
    }
}

void rwSpinLock_upunlock_rdlock(rwSpinLock_tt* self)
{
    atomic_fetch_add_explicit(&((*self)->iBits), RW_READER - RW_UPGRADED, memory_order_acq_rel);
}

void rwSpinLock_wrunlock_uplock(rwSpinLock_tt* self)
{
    atomic_fetch_or_explicit(&((*self)->iBits), RW_UPGRADED, memory_order_acquire);
    atomic_fetch_add_explicit(&((*self)->iBits), -RW_WRITER, memory_order_release);
}

bool rwSpinLock_tryupunlock_wrlock(rwSpinLock_tt* self)
{
    return tryupunlock_wrlock(self);
}
