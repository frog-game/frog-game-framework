

#include "channel/channelCenter_t.h"

#include "channel/channel_t.h"
#include "rwSpinLock_t.h"
#include "thread_t.h"
#include "utility_t.h"

#include <stdatomic.h>
#include <stdlib.h>

#define DEF_USE_SPINLOCK

#define def_channelHandleMask 0xfffff

static inline size_t Min(const size_t a, const size_t b)
{
    return a < b ? a : b;
}

typedef struct channelCenter_s
{
#ifdef DEF_USE_SPINLOCK
    rwSpinLock_tt rwlock;
#else
    rwlock_tt rwlock;
#endif
    channel_tt** ppChannelHandleSlot;
    int32_t      iSequence;
    int32_t      iChannelHandleSlotCapacity;
    int32_t*     pChannelHandleSlotIndex;
    int32_t      iChannelHandleSlotIndexCount;
} channelCenter_tt;

static channelCenter_tt* s_pChannelCenter = NULL;

void channelCenter_init()
{
    if (s_pChannelCenter == NULL) {
        channelCenter_tt* pChannelCenter = mem_malloc(sizeof(channelCenter_tt));
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_init(&pChannelCenter->rwlock);
#else
        rwlock_init(&pChannelCenter->rwlock);
#endif
        pChannelCenter->iSequence                  = 1;
        pChannelCenter->iChannelHandleSlotCapacity = 64;
        pChannelCenter->ppChannelHandleSlot =
            mem_malloc(pChannelCenter->iChannelHandleSlotCapacity * sizeof(channel_tt*));
        bzero(pChannelCenter->ppChannelHandleSlot,
              pChannelCenter->iChannelHandleSlotCapacity * sizeof(channel_tt*));

        pChannelCenter->iChannelHandleSlotIndexCount = pChannelCenter->iChannelHandleSlotCapacity;
        pChannelCenter->pChannelHandleSlotIndex =
            mem_malloc(pChannelCenter->iChannelHandleSlotIndexCount * sizeof(int32_t));
        for (int32_t i = 0; i < pChannelCenter->iChannelHandleSlotIndexCount; ++i) {
            pChannelCenter->pChannelHandleSlotIndex[i] =
                pChannelCenter->iChannelHandleSlotCapacity - i - 1;
        }

        s_pChannelCenter = pChannelCenter;
    }
}

void channelCenter_clear()
{
    if (s_pChannelCenter != NULL) {
        channelCenter_tt* pChannelCenter = s_pChannelCenter;
        s_pChannelCenter                 = NULL;

        if (pChannelCenter->ppChannelHandleSlot) {
            for (int32_t i = 0; i < pChannelCenter->iChannelHandleSlotCapacity; ++i) {
                if (pChannelCenter->ppChannelHandleSlot[i] != NULL) {
                    channelCenter_deregister(channel_getID(pChannelCenter->ppChannelHandleSlot[i]));
                }
            }
            mem_free(pChannelCenter->ppChannelHandleSlot);
            pChannelCenter->ppChannelHandleSlot = NULL;
        }

        if (pChannelCenter->pChannelHandleSlotIndex) {
            mem_free(pChannelCenter->pChannelHandleSlotIndex);
            pChannelCenter->pChannelHandleSlotIndex = NULL;
        }

#ifndef DEF_USE_SPINLOCK
        rwlock_destroy(&pChannelCenter->rwlock);
#endif
        mem_free(pChannelCenter);
    }
}

uint32_t channelCenter_register(struct channel_s* pHandle)
{
    channelCenter_tt* pChannelCenter = s_pChannelCenter;
    if (pChannelCenter) {
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_wrlock(&pChannelCenter->rwlock);
#else
        rwlock_wrlock(&pChannelCenter->rwlock);
#endif
        if (pChannelCenter->iChannelHandleSlotIndexCount == 0) {
            if (pChannelCenter->iChannelHandleSlotCapacity == 0xfffff) {
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrunlock(&pChannelCenter->rwlock);
#else
                rwlock_wrunlock(&pChannelCenter->rwlock);
#endif
                return 0;
            }

            int32_t iOldChannelHandleSlotCapacity = pChannelCenter->iChannelHandleSlotCapacity;
            pChannelCenter->iChannelHandleSlotCapacity =
                (int32_t)Min(iOldChannelHandleSlotCapacity * 2, 0xfffff);
            int32_t iAddChannelHandleSlotCount =
                pChannelCenter->iChannelHandleSlotCapacity - iOldChannelHandleSlotCapacity;
            pChannelCenter->ppChannelHandleSlot =
                mem_realloc(pChannelCenter->ppChannelHandleSlot,
                            pChannelCenter->iChannelHandleSlotCapacity * sizeof(channel_tt*));
            bzero(pChannelCenter->ppChannelHandleSlot + iOldChannelHandleSlotCapacity,
                  iAddChannelHandleSlotCount * sizeof(channel_tt*));

            mem_free(pChannelCenter->pChannelHandleSlotIndex);
            pChannelCenter->pChannelHandleSlotIndex =
                mem_malloc(pChannelCenter->iChannelHandleSlotCapacity * sizeof(int32_t));
            pChannelCenter->iChannelHandleSlotIndexCount = iAddChannelHandleSlotCount;

            for (int32_t i = 0; i < pChannelCenter->iChannelHandleSlotIndexCount; ++i) {
                pChannelCenter->pChannelHandleSlotIndex[i] =
                    pChannelCenter->iChannelHandleSlotCapacity - i - 1;
            }
        }

        --pChannelCenter->iChannelHandleSlotIndexCount;
        int32_t iIndex =
            pChannelCenter->pChannelHandleSlotIndex[pChannelCenter->iChannelHandleSlotIndexCount];
        channel_addref(pHandle);
        pChannelCenter->ppChannelHandleSlot[iIndex] = pHandle;
        int32_t iSequence                           = pChannelCenter->iSequence << 20;
        ++pChannelCenter->iSequence;
        if (pChannelCenter->iSequence == 0x7ff) {
            pChannelCenter->iSequence = 1;
        }
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_wrunlock(&pChannelCenter->rwlock);
#else
        rwlock_wrunlock(&pChannelCenter->rwlock);
#endif
        return ((iIndex + 1) | 0x80000000 | iSequence);
    }
    return 0;
}

bool channelCenter_deregister(uint32_t uiChannelID)
{
    if (uiChannelID & 0x80000000) {
        channelCenter_tt* pChannelCenter = s_pChannelCenter;
        if (pChannelCenter) {
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_wrlock(&pChannelCenter->rwlock);
#else
            rwlock_wrlock(&pChannelCenter->rwlock);
#endif
            int32_t iIndex = ((uiChannelID & def_channelHandleMask) - 1);
            if (iIndex < pChannelCenter->iChannelHandleSlotCapacity) {
                channel_tt* pChannelHandle = pChannelCenter->ppChannelHandleSlot[iIndex];
                if (pChannelHandle && channel_getID(pChannelHandle) == uiChannelID) {
                    channel_release(pChannelHandle);
                    pChannelCenter->ppChannelHandleSlot[iIndex] = NULL;
                    pChannelCenter
                        ->pChannelHandleSlotIndex[pChannelCenter->iChannelHandleSlotIndexCount] =
                        iIndex;
                    ++(pChannelCenter->iChannelHandleSlotIndexCount);
#ifdef DEF_USE_SPINLOCK
                    rwSpinLock_wrunlock(&pChannelCenter->rwlock);
#else
                    rwlock_wrunlock(&pChannelCenter->rwlock);
#endif
                    return true;
                }
            }
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_wrunlock(&pChannelCenter->rwlock);
#else
            rwlock_wrunlock(&pChannelCenter->rwlock);
#endif
        }
    }

    return false;
}

channel_tt* channelCenter_gain(uint32_t uiChannelID)
{
    if (uiChannelID & 0x80000000) {
        channelCenter_tt* pChannelCenter = s_pChannelCenter;
        if (pChannelCenter) {
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_rdlock(&pChannelCenter->rwlock);
#else
            rwlock_rdlock(&pChannelCenter->rwlock);
#endif
            int32_t iIndex = ((uiChannelID & def_channelHandleMask) - 1);
            if (iIndex < pChannelCenter->iChannelHandleSlotCapacity) {
                channel_tt* pChannelHandle = pChannelCenter->ppChannelHandleSlot[iIndex];
                if (pChannelHandle && channel_getID(pChannelHandle) == uiChannelID) {
                    channel_addref(pChannelHandle);
#ifdef DEF_USE_SPINLOCK
                    rwSpinLock_rdunlock(&pChannelCenter->rwlock);
#else
                    rwlock_rdunlock(&pChannelCenter->rwlock);
#endif
                    return pChannelHandle;
                }
            }
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_rdunlock(&pChannelCenter->rwlock);
#else
            rwlock_rdunlock(&pChannelCenter->rwlock);
#endif
        }
    }

    return NULL;
}

uint32_t* channelCenter_getIds(int32_t* pCount)
{
    channelCenter_tt* pChannelCenter = s_pChannelCenter;
    if (pChannelCenter) {
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_rdlock(&pChannelCenter->rwlock);
#else
        rwlock_rdlock(&pChannelCenter->rwlock);
#endif
        *pCount = pChannelCenter->iChannelHandleSlotCapacity -
                  pChannelCenter->iChannelHandleSlotIndexCount;
        if (*pCount > 0) {
            uint32_t* pChannelIDs = mem_malloc(sizeof(uint32_t) * *pCount);
            int32_t   iIndex      = 0;
            for (int32_t i = 0; i < pChannelCenter->iChannelHandleSlotCapacity; ++i) {
                if (pChannelCenter->ppChannelHandleSlot[i] != NULL) {
                    pChannelIDs[iIndex] = channel_getID(pChannelCenter->ppChannelHandleSlot[i]);
                    ++iIndex;
                }
            }
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_rdunlock(&pChannelCenter->rwlock);
#else
            rwlock_rdunlock(&pChannelCenter->rwlock);
#endif
            return pChannelIDs;
        }
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_rdunlock(&pChannelCenter->rwlock);
#else
        rwlock_rdunlock(&pChannelCenter->rwlock);
#endif
    }
    *pCount = 0;
    return NULL;
}
