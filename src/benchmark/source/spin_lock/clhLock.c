#include "clhLock.h"

#include <stdatomic.h>

#include "thread_t.h"
#include "utility_t.h"

typedef struct _decl_cpu_cache_align chlNode_s
{
    bool bLocked;
    // atomic_bool bLocked;
} chlNode_tt;

struct clhLock_s
{
    chlNode_tt*          pSelfNode;
    _Atomic(chlNode_tt*) hTailNode;
};

static inline chlNode_tt* createChlNode(bool locked)
{
    chlNode_tt* pNode = malloc(sizeof(chlNode_tt));
    // atomic_init(&pNode->bLocked,locked);
    return pNode;
}

void clhLock_init(clhLock_tt* self)
{
    *self              = malloc(sizeof(struct clhLock_s));
    chlNode_tt* pNode  = createChlNode(false);
    pNode->bLocked     = false;
    (*self)->pSelfNode = pNode;
    atomic_init(&((*self)->hTailNode), (*self)->pSelfNode);
    //(*self)->pLockNode = NULL;
}

void clhLock_destroy(clhLock_tt* self)
{
    free((*self)->pSelfNode);
    free(*self);
    *self = NULL;
}

static void tls_chlNode_cleanup_func(void* pArg)
{
    free(pArg);
}

static _decl_threadLocal chlNode_tt* s_pLocalNode = NULL;
static _decl_threadLocal chlNode_tt* s_pPrevNode  = NULL;

void clhLock_lock(clhLock_tt* self)
{
    if (s_pLocalNode == NULL) {
        s_pLocalNode = createChlNode(true);
        setTlsValue(*self, tls_chlNode_cleanup_func, s_pLocalNode, true);
    }

    s_pLocalNode->bLocked = true;

    s_pPrevNode = (chlNode_tt*)atomic_exchange(&((*self)->hTailNode), s_pLocalNode);
    while (s_pPrevNode->bLocked) {
        threadYield();
    }

    // while(s_pPrevNode->bLocked)
    //{
    //     cpu_relax();
    //     if(!s_pPrevNode->bLocked)
    //     {
    //         break;
    //     }
    //     threadYield();
    // }
}

void clhLock_unlock(clhLock_tt* self)
{
    (*self)->pSelfNode = s_pLocalNode;
    s_pLocalNode       = s_pPrevNode;
    setTlsValue(*self, tls_chlNode_cleanup_func, s_pLocalNode, false);
    (*self)->pSelfNode->bLocked = false;
}
