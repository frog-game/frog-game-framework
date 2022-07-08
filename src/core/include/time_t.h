#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "platform_t.h"

typedef struct timespec_s
{
    int32_t iSec;
    int32_t iNsec;
} timespec_tt;

#define timespec_cmp(pTvp, pUvp, cmp)                                  \
    (((pTvp)->iSec == (pUvp)->iSec) ? ((pTvp)->iNsec cmp(pUvp)->iNsec) \
                                    : ((pTvp)->iSec cmp(pUvp)->iSec))

#define timespec_add(pTvp, pUvp, pVvp)                 \
    {                                                  \
        (pVvp)->iSec  = (pTvp)->iSec + (pUvp)->iSec;   \
        (pVvp)->iNsec = (pTvp)->iNsec + (pUvp)->iNsec; \
        if ((pVvp)->iNsec > 1000000000l) {             \
            ++((pVvp)->iSec);                          \
            (pVvp)->iNsec -= 1000000000l;              \
        }                                              \
    }

#define timespec_sub(pTvp, pUvp, pVvp)                 \
    {                                                  \
        (pVvp)->iSec  = (pTvp)->iSec - (pUvp)->iSec;   \
        (pVvp)->iNsec = (pTvp)->iNsec - (pUvp)->iNsec; \
        if ((pVvp)->iNsec < 0) {                       \
            --((pVvp)->iSec);                          \
            (pVvp)->iNsec += 1000000000l;              \
        }                                              \
    }

#define timespec_clear(pTs) ((pTs)->iSec = (pTs)->iNsec = 0)

#define timespec_zero(pTs) ((pTs)->iSec == 0) && ((pTs)->iNsec == 0)

#define timespec_toMsec(pTs) (((int64_t)(pTs)->iSec * 1000) + (((pTs)->iNsec + 999999) / 1000000))

#define timespec_toNsec(pTs) (((int64_t)(pTs)->iSec) * 1000000000L + (pTs)->iNsec)

#define timespec_addToMs(pTvp, pUvp)                              \
    (((((int64_t)(pTvp)->iSec + (int64_t)(pUvp)->iSec)) * 1000) + \
     (((pTvp)->iNsec + (pUvp)->iNsec + 999999) / 1000000))

#define timespec_subToMs(pTvp, pUvp)                            \
    ((((int64_t)(pTvp)->iSec - (int64_t)(pUvp)->iSec) * 1000) + \
     (((pTvp)->iNsec - (pUvp)->iNsec + 999999) / 1000000))

#define timespec_addToNs(pTvp, pUvp)                                 \
    (((int64_t)(pTvp)->iSec + (int64_t)(pUvp)->iSec) * 1000000000L + \
     ((pTvp)->iNsec + (pUvp)->iNsec))

#define timespec_subToNs(pTvp, pUvp)                                 \
    (((int64_t)(pTvp)->iSec - (int64_t)(pUvp)->iSec) * 1000000000L + \
     ((pTvp)->iNsec - (pUvp)->iNsec))

frCore_API void getClockRealtime(timespec_tt* pTs);

frCore_API void getClockMonotonic(timespec_tt* pTs);
