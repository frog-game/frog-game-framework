#ifndef Frog_mscLock_h
#define Frog_mscLock_h

struct mscLock_s;
typedef struct mscLock_s* mscLock_tt;

void mscLock_init(mscLock_tt* self);

void mscLock_destroy(mscLock_tt* self);

void mscLock_lock(mscLock_tt* self);

void mscLock_unlock(mscLock_tt* self);

#endif
