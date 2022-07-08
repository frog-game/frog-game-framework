#ifndef _spinLock_h
#define _spinLock_h

struct spinLock_s;
typedef struct spinLock_s* spinLock_tt;

void spinLock_init(spinLock_tt* lock);

void spinLock_destroy(spinLock_tt* lock);

void spinLock_lock(spinLock_tt* lock);

void spinLock_lock2(spinLock_tt* lock);

void spinLock_lock3(spinLock_tt* lock);

void spinLock_unlock(spinLock_tt* lock);

#endif
