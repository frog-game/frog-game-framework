

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define CARES_STATICLIB 1
#include "ares.h"

#include "eventIO/eventIO_t.h"

struct dnsResolve_s
{
    struct service_s*            pService;
    ares_channel                 aresCtx;
    uint32_t                     uiToken;
    uint32_t                     uiTimeoutMs;
    char*                        szAddressList;
    int32_t                      iAddressCount;
    const char*                  pRecvBuffer;
    size_t                       nRecvLength;
    size_t                       nRecvOffset;
    inetAddress_tt               recvAddress;
    ioBufVec_tt                  sendBufV[8];
    int32_t                      iBufVCount;
    _Atomic(eventConnection_tt*) hConnection;
    _Atomic(eventTimer_tt*)      hTimeout;
    atomic_bool                  bActive;
    atomic_int                   iRefCount;
};
