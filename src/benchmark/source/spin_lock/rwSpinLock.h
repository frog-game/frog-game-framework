#ifndef _rwSpinLock_h
#define _rwSpinLock_h

#include <stdbool.h>

struct rwSpinLock_s;
typedef struct rwSpinLock_s* rwSpinLock_tt;

void rwSpinLock_init(rwSpinLock_tt* self);

void rwSpinLock_destroy(rwSpinLock_tt* self);

void rwSpinLock_rdlock(rwSpinLock_tt* self);

bool rwSpinLock_tryrdlock(rwSpinLock_tt* self);

void rwSpinLock_rdunlock(rwSpinLock_tt* self);

void rwSpinLock_wrlock(rwSpinLock_tt* self);

bool rwSpinLock_trywrlock(rwSpinLock_tt* self);

void rwSpinLock_wrunlock(rwSpinLock_tt* self);

void rwSpinLock_wrunlock_rdlock(rwSpinLock_tt* self);

void rwSpinLock_upLock(rwSpinLock_tt* self);

void rwSpinLock_upunLock(rwSpinLock_tt* self);

bool rwSpinLock_tryuplock(rwSpinLock_tt* self);

void rwSpinLock_upunlock_wrlock(rwSpinLock_tt* self);

void rwSpinLock_upunlock_rdlock(rwSpinLock_tt* self);

void rwSpinLock_wrunlock_uplock(rwSpinLock_tt* self);

bool rwSpinLock_tryupunlock_wrlock(rwSpinLock_tt* self);

#endif