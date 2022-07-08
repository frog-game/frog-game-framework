#include "mscLock.h"

#include <stdatomic.h>

#include "thread_t.h"
#include "utility_t.h"

typedef struct _decl_cpu_cache_align mscNode_s
{
    volatile struct mscNode_s* pNext;
    volatile bool              bLocked;
} mscNode_tt;

struct mscLock_s
{
    _Atomic(mscNode_tt*) pTailLock;
};

static _decl_threadLocal mscNode_tt* s_pLocalNode = NULL;


static inline mscNode_tt* createMscNode()
{
    mscNode_tt* pNode = malloc(sizeof(mscNode_tt));
    pNode->bLocked    = false;
    pNode->pNext      = NULL;
    return pNode;
}


void mscLock_init(mscLock_tt* self)
{
    *self = malloc(sizeof(struct mscLock_s));
    atomic_init(&(*self)->pTailLock, NULL);
}

void mscLock_destroy(mscLock_tt* self)
{
    free(*self);
    *self = NULL;
}


static void tls_mscNode_cleanup_func(void* pArg)
{
    free(pArg);
}

void mscLock_lock(mscLock_tt* self)
{
    if (s_pLocalNode == NULL) {
        s_pLocalNode = createMscNode();
        setTlsValue(*self, tls_mscNode_cleanup_func, s_pLocalNode, true);
    }

    s_pLocalNode->pNext   = NULL;
    s_pLocalNode->bLocked = true;

    mscNode_tt* pPrevNode = (mscNode_tt*)atomic_exchange(&((*self)->pTailLock), s_pLocalNode);
    if (pPrevNode == NULL) {
        return;
    }

    pPrevNode->pNext = s_pLocalNode;
    while (s_pLocalNode->bLocked) {
        threadYield();
    }
}

void mscLock_unlock(mscLock_tt* self)
{
    mscNode_tt* tmp = s_pLocalNode;
    if (s_pLocalNode->pNext == NULL) {
        if (atomic_compare_exchange_strong(&((*self)->pTailLock), &tmp, NULL)) {
            return;
        }
        while (s_pLocalNode->pNext == NULL) {
            threadYield();
        }
    }
    s_pLocalNode->pNext->bLocked = false;
}
