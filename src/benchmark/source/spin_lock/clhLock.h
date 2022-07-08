#ifndef Frog_clhLock_h
#define Frog_clhLock_h

struct clhLock_s;
typedef struct clhLock_s* clhLock_tt;

void clhLock_init(clhLock_tt* self);

void clhLock_destroy(clhLock_tt* self);

void clhLock_lock(clhLock_tt* self);

void clhLock_unlock(clhLock_tt* self);

#endif
