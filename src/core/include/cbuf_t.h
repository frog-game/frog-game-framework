#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "utility_t.h"

typedef struct cbuf_s
{
    // private
    char*  pBuffer;
    size_t nLength;
} cbuf_tt;

static inline void cbuf_init(cbuf_tt* pBuf, size_t nLength)
{
    pBuf->nLength = nLength;
    if (nLength != 0) {
        pBuf->pBuffer = (char*)mem_malloc(nLength);
    }
    else {
        pBuf->pBuffer = NULL;
    }
}

static inline void cbuf_clear(cbuf_tt* pBuf)
{
    pBuf->nLength = 0;
    if (pBuf->pBuffer) {
        mem_free(pBuf->pBuffer);
        pBuf->pBuffer = NULL;
    }
}

static inline char* cbuf_buffer(cbuf_tt* pBuf)
{
    return pBuf->pBuffer;
}

static inline size_t cbuf_length(cbuf_tt* pBuf)
{
    return pBuf->nLength;
}

static inline bool cbuf_empty(cbuf_tt* pBuf)
{
    return pBuf->nLength == 0;
}

static inline void cbuf_swapToReset(cbuf_tt* pBuf, cbuf_tt* pRhs)
{
    char*  pBuffer = pBuf->pBuffer;
    size_t nLength = pBuf->nLength;
    pBuf->pBuffer  = pRhs->pBuffer;
    pBuf->nLength  = pRhs->nLength;
    pRhs->pBuffer  = pBuffer;
    pRhs->nLength  = nLength;
}

static inline void cbuf_swap(cbuf_tt* pBuf, char** ppBuffer, size_t* pLength)
{
    char*  pBuffer = pBuf->pBuffer;
    size_t nLength = pBuf->nLength;
    pBuf->pBuffer  = *ppBuffer;
    pBuf->nLength  = *pLength;
    *ppBuffer      = pBuffer;
    *pLength       = nLength;
}
