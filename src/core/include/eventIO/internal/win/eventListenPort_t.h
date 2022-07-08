

#pragma once

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <stdatomic.h>

#include "utility_t.h"
#include "rbtree_t.h"

#include "inetAddress_t.h"
#include "rwSpinLock_t.h"

#ifndef MAXIMUM_MTU_SIZE
#    define MAXIMUM_MTU_SIZE 1492
#endif

struct eventConnection_s;

typedef struct acceptOverlappedPlus_s
{
    OVERLAPPED                _Overlapped;
    struct eventListenPort_s* pEventListenPort;
    char                      szBuffer[128];
    SOCKET                    hSocket;
} acceptOverlappedPlus_tt;

typedef struct recvFromOverlappedPlus_s
{
    OVERLAPPED                _Overlapped;
    struct eventListenPort_s* pEventListenPort;
    inetAddress_tt            inetAddress;
    int32_t                   iAddrLen;
    char                      szBuffer[MAXIMUM_MTU_SIZE];
} recvFromOverlappedPlus_tt;

typedef struct addressConnection_s
{
    RB_ENTRY(addressConnection_s)
    entry;
    inetAddress_tt            inetAddress;
    uint64_t                  hash;
    struct eventConnection_s* pConnection;
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
    SOCKET                  hSocket;
    struct eventIO_s*       pEventIO;
    inetAddress_tt          listenAddr;
    addressConnectionMap_tt mapAddressConnection;
    rwSpinLock_tt           rwlock;
    bool                    bTcp;
    atomic_bool             bActive;
    atomic_int              iRefCount;
};

__UNUSED void eventListenPort_onAccept(struct eventListenPort_s* pHandle,
                                       acceptOverlappedPlus_tt*  pOverlappedPlus,
                                       uint32_t                  uiTransferred);

__UNUSED void eventListenPort_onRecvFrom(struct eventListenPort_s*  pHandle,
                                         recvFromOverlappedPlus_tt* pOverlappedPlus,
                                         uint32_t                   uiTransferred);

__UNUSED void eventListenPort_removeAddressConnection(struct eventListenPort_s* pHandle,
                                                      const inetAddress_tt*     pRemoteAddr);
