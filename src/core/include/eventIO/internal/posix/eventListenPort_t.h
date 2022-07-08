#pragma once

#include <stdatomic.h>

#include "utility_t.h"
#include "rbtree_t.h"

#include "inetAddress_t.h"
#include "rwSpinLock_t.h"

#include "eventIO/internal/posix/poller_t.h"

struct eventIOLoop_s;
struct eventConnection_s;
struct eventListenPort_s;

typedef struct listenHandle_s
{
    pollHandle_tt             pollHandle;
    int32_t                   hSocket;
    struct eventListenPort_s* pEventListenPort;
    struct eventIOLoop_s*     pEventIOLoop;
} listenHandle_tt;

typedef struct addressConnection_s
{
    RB_ENTRY(addressConnection_s)
    entry;
    inetAddress_tt inetAddress;
    uint64_t       hash;
} addressConnection_tt;

static inline int32_t addressCmp(struct addressConnection_s* src, struct addressConnection_s* dst)
{
    int32_t iComp = (src->hash < dst->hash ? -1 : src->hash > dst->hash ? 1 : 0);
    if (iComp != 0) {
        return iComp;
    }
    return inetAddressCmp(&src->inetAddress, &dst->inetAddress);
}

RB_HEAD(addressConnectionMap_s, addressConnection_s);
RB_GENERATE_STATIC(addressConnectionMap_s, addressConnection_s, entry, addressCmp)

typedef struct addressConnectionMap_s addressConnectionMap_tt;

struct eventListenPort_s
{
    void (*fnAcceptCallback)(struct eventListenPort_s*, struct eventConnection_s*, const char*,
                             uint32_t, void*);
    void (*fnUserFree)(void*);
    void*                   pUserData;
    listenHandle_tt*        pListenHandle;
    struct eventIO_s*       pEventIO;
    inetAddress_tt          listenAddr;
    addressConnectionMap_tt mapAddressConnection;
    rwSpinLock_tt           rwlock;
    bool                    bTcp;
    atomic_bool             bActive;
    atomic_int              iRefCount;
};

__UNUSED void eventListenPort_removeAddressConnection(struct eventListenPort_s* pHandle,
                                                      const inetAddress_tt*     pRemoteAddr);
