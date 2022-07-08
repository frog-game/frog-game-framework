

#include "serviceCenter_t.h"

#include "rbtree_t.h"
#include "rwSpinLock_t.h"
#include "service_t.h"
#include "thread_t.h"
#include "utility_t.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#define DEF_USE_SPINLOCK

#define def_serviceHandleMask 0xfffff

static inline size_t Min(const size_t a, const size_t b)
{
    return a < b ? a : b;
}

typedef struct service_name_s
{
    RB_ENTRY(service_name_s)
    entry;
    char*    szName;
    uint32_t uiServiceID;
} service_name_tt;

static inline int32_t serviceNameNodeCmp(struct service_name_s* pNode1,
                                         struct service_name_s* pNode2)
{
    int32_t iRet = strcmp(pNode1->szName, pNode2->szName);
    return (iRet < 0 ? -1 : iRet > 0 ? 1 : 0);
}

RB_HEAD(service_name_tree_s, service_name_s);
RB_GENERATE_STATIC(service_name_tree_s, service_name_s, entry, serviceNameNodeCmp)

typedef struct service_name_tree_s service_name_tree_tt;

typedef struct serviceCenter_s
{
#ifdef DEF_USE_SPINLOCK
    rwSpinLock_tt rwlock;
#else
    rwlock_tt rwlock;
#endif
    int32_t              iServerNodeId;
    uint32_t             uiServerNodeMask;
    service_tt**         ppServiceHandleSlot;
    int32_t              iServiceHandleSlotCapacity;
    int32_t*             pServiceHandleSlotIndex;
    int32_t              iServiceHandleSlotIndexCount;
    service_name_tree_tt serviceNameTree;

} serviceCenter_tt;

static serviceCenter_tt* s_pServiceCenter = NULL;

void serviceCenter_init(int32_t iServerNodeId)
{
    if (s_pServiceCenter == NULL) {
        serviceCenter_tt* pServiceCenter = mem_malloc(sizeof(serviceCenter_tt));

        pServiceCenter->iServiceHandleSlotCapacity = 64;
        pServiceCenter->ppServiceHandleSlot =
            mem_malloc(pServiceCenter->iServiceHandleSlotCapacity * sizeof(service_tt*));
        bzero(pServiceCenter->ppServiceHandleSlot,
              pServiceCenter->iServiceHandleSlotCapacity * sizeof(service_tt*));
        pServiceCenter->iServiceHandleSlotIndexCount = pServiceCenter->iServiceHandleSlotCapacity;
        pServiceCenter->pServiceHandleSlotIndex =
            mem_malloc(pServiceCenter->iServiceHandleSlotIndexCount * sizeof(int32_t));
        for (int32_t i = 0; i < pServiceCenter->iServiceHandleSlotIndexCount; ++i) {
            pServiceCenter->pServiceHandleSlotIndex[i] =
                pServiceCenter->iServiceHandleSlotCapacity - i - 1;
        }

        RB_INIT(&pServiceCenter->serviceNameTree);

        pServiceCenter->uiServerNodeMask = (iServerNodeId & 0x7ff) << 20;
        pServiceCenter->iServerNodeId    = iServerNodeId;

#ifdef DEF_USE_SPINLOCK
        rwSpinLock_init(&pServiceCenter->rwlock);
#else
        rwlock_init(&pServiceCenter->rwlock);
#endif
        s_pServiceCenter = pServiceCenter;
    }
}

uint32_t serviceCenter_register(struct service_s* pServiceHandle)
{
    serviceCenter_tt* pServiceCenter = s_pServiceCenter;
    if (pServiceCenter) {
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_wrlock(&pServiceCenter->rwlock);
#else
        rwlock_wrlock(&pServiceCenter->rwlock);
#endif
        if (pServiceCenter->iServiceHandleSlotIndexCount == 0) {
            if (pServiceCenter->iServiceHandleSlotCapacity == 0xfffff) {
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
                rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
                return 0;
            }

            int32_t iOldServiceHandleSlotCapacity = pServiceCenter->iServiceHandleSlotCapacity;
            pServiceCenter->iServiceHandleSlotCapacity =
                (int32_t)Min(iOldServiceHandleSlotCapacity * 2, 0xfffff);
            pServiceCenter->ppServiceHandleSlot =
                mem_realloc(pServiceCenter->ppServiceHandleSlot,
                            pServiceCenter->iServiceHandleSlotCapacity * sizeof(service_tt*));
            int32_t iAddServiceHandleSlotCount =
                pServiceCenter->iServiceHandleSlotCapacity - iOldServiceHandleSlotCapacity;

            mem_free(pServiceCenter->pServiceHandleSlotIndex);
            pServiceCenter->pServiceHandleSlotIndex =
                mem_malloc(pServiceCenter->iServiceHandleSlotCapacity * sizeof(int32_t));
            pServiceCenter->iServiceHandleSlotIndexCount = iAddServiceHandleSlotCount;

            for (int32_t i = 0; i < pServiceCenter->iServiceHandleSlotIndexCount; ++i) {
                pServiceCenter->pServiceHandleSlotIndex[i] =
                    pServiceCenter->iServiceHandleSlotCapacity - i - 1;
            }
        }

        --pServiceCenter->iServiceHandleSlotIndexCount;
        int32_t iIndex =
            pServiceCenter->pServiceHandleSlotIndex[pServiceCenter->iServiceHandleSlotIndexCount];
        service_addref(pServiceHandle);
        pServiceCenter->ppServiceHandleSlot[iIndex] = pServiceHandle;
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
        rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
        return pServiceCenter->uiServerNodeMask | (iIndex + 1);
    }
    return 0;
}

static inline bool serviceCenter_removeName(uint32_t uiServiceID)
{
    if (uiServiceID != 0) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        if (pServiceCenter) {
            service_name_tt* pNode;
            RB_FOREACH(pNode, service_name_tree_s, &pServiceCenter->serviceNameTree)
            {
                if (pNode->uiServiceID == uiServiceID) {
                    RB_REMOVE(service_name_tree_s, &pServiceCenter->serviceNameTree, pNode);
                    mem_free(pNode->szName);
                    mem_free(pNode);
                    return true;
                }
            }
        }
    }
    return false;
}

bool serviceCenter_deregister(uint32_t uiServiceID)
{
    if (uiServiceID != 0) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        if (pServiceCenter) {
            if (uiServiceID & pServiceCenter->uiServerNodeMask) {
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrlock(&pServiceCenter->rwlock);
#else
                rwlock_wrlock(&pServiceCenter->rwlock);
#endif
                int32_t iIndex = (uiServiceID & def_serviceHandleMask) - 1;
                if (iIndex < pServiceCenter->iServiceHandleSlotCapacity) {
                    service_tt* pServiceHandle = pServiceCenter->ppServiceHandleSlot[iIndex];
                    if (pServiceHandle && service_getID(pServiceHandle) == uiServiceID) {
                        serviceCenter_removeName(uiServiceID);
                        service_release(pServiceHandle);
                        pServiceCenter->ppServiceHandleSlot[iIndex] = NULL;
                        pServiceCenter->pServiceHandleSlotIndex
                            [pServiceCenter->iServiceHandleSlotIndexCount] = iIndex;
                        ++(pServiceCenter->iServiceHandleSlotIndexCount);
#ifdef DEF_USE_SPINLOCK
                        rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
                        rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
                        return true;
                    }
                }
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
                rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
            }
        }
    }
    return false;
}

void serviceCenter_clear()
{
    if (s_pServiceCenter != NULL) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        s_pServiceCenter                 = NULL;

        if (pServiceCenter->ppServiceHandleSlot) {
            for (int32_t i = 0; i < pServiceCenter->iServiceHandleSlotCapacity; ++i) {
                if (pServiceCenter->ppServiceHandleSlot[i] != NULL) {
                    serviceCenter_deregister(service_getID(pServiceCenter->ppServiceHandleSlot[i]));
                }
            }
            mem_free(pServiceCenter->ppServiceHandleSlot);
            pServiceCenter->ppServiceHandleSlot = NULL;
        }

        if (pServiceCenter->pServiceHandleSlotIndex) {
            mem_free(pServiceCenter->pServiceHandleSlotIndex);
            pServiceCenter->pServiceHandleSlotIndex = NULL;
        }
#ifndef DEF_USE_SPINLOCK
        rwlock_destroy(&pServiceCenter->rwlock);
#endif
        mem_free(pServiceCenter);
    }
}

service_tt* serviceCenter_gain(uint32_t uiServiceID)
{
    if (uiServiceID != 0) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        if (pServiceCenter) {
            if (uiServiceID & pServiceCenter->uiServerNodeMask) {
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_rdlock(&pServiceCenter->rwlock);
#else
                rwlock_rdlock(&pServiceCenter->rwlock);
#endif
                int32_t iIndex = (uiServiceID & def_serviceHandleMask) - 1;
                if (iIndex < pServiceCenter->iServiceHandleSlotCapacity) {
                    service_tt* pServiceHandle = pServiceCenter->ppServiceHandleSlot[iIndex];
                    if (pServiceHandle && service_getID(pServiceHandle) == uiServiceID) {
                        service_addref(pServiceHandle);
#ifdef DEF_USE_SPINLOCK
                        rwSpinLock_rdunlock(&pServiceCenter->rwlock);
#else
                        rwlock_rdunlock(&pServiceCenter->rwlock);
#endif
                        return pServiceHandle;
                    }
                }
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_rdunlock(&pServiceCenter->rwlock);
#else
                rwlock_rdunlock(&pServiceCenter->rwlock);
#endif
            }
        }
    }
    return NULL;
}

bool serviceCenter_bindName(uint32_t uiServiceID, const char* szName)
{
    if (!(uiServiceID & 0x80000000)) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        if (pServiceCenter) {
            if (uiServiceID & pServiceCenter->uiServerNodeMask) {
                service_name_tt* pNode = mem_malloc(sizeof(service_name_tt));
                pNode->szName          = mem_strdup(szName);
                pNode->uiServiceID     = uiServiceID;
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrlock(&pServiceCenter->rwlock);
#else
                rwlock_wrlock(&pServiceCenter->rwlock);
#endif
                if (RB_INSERT(service_name_tree_s, &pServiceCenter->serviceNameTree, pNode) ==
                    NULL) {
#ifdef DEF_USE_SPINLOCK
                    rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
                    rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
                    return true;
                }
#ifdef DEF_USE_SPINLOCK
                rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
                rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
                mem_free(pNode);
            }
        }
    }
    return false;
}

bool serviceCenter_unbindName(uint32_t uiServiceID)
{
    bool bRemove = false;
    if (uiServiceID != 0) {
        serviceCenter_tt* pServiceCenter = s_pServiceCenter;
        if (pServiceCenter) {
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_wrlock(&pServiceCenter->rwlock);
#else
            rwlock_wrlock(&pServiceCenter->rwlock);
#endif
            bRemove = serviceCenter_removeName(uiServiceID);
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_wrunlock(&pServiceCenter->rwlock);
#else
            rwlock_wrunlock(&pServiceCenter->rwlock);
#endif
        }
    }
    return bRemove;
}

uint32_t serviceCenter_findServiceID(const char* szName)
{
    uint32_t          uiServiceID    = 0;
    serviceCenter_tt* pServiceCenter = s_pServiceCenter;
    if (pServiceCenter) {
        service_name_tt dataNode;
        dataNode.szName = (char*)szName;
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_rdlock(&pServiceCenter->rwlock);
#else
        rwlock_rdlock(&pServiceCenter->rwlock);
#endif
        service_name_tt* pNode =
            RB_FIND(service_name_tree_s, &pServiceCenter->serviceNameTree, &dataNode);
        if (pNode == NULL) {
#ifdef DEF_USE_SPINLOCK
            rwSpinLock_rdunlock(&pServiceCenter->rwlock);
#else
            rwlock_rdunlock(&pServiceCenter->rwlock);
#endif
            return 0;
        }
        uiServiceID = pNode->uiServiceID;
#ifdef DEF_USE_SPINLOCK
        rwSpinLock_rdunlock(&pServiceCenter->rwlock);
#else
        rwlock_rdunlock(&pServiceCenter->rwlock);
#endif
    }
    return uiServiceID;
}
