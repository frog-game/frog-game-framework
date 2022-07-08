#pragma once

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "utility_t.h"

typedef struct slice_s
{
    // private
    const char* szData;
    size_t      nLength;
} slice_tt;

static inline void slice_clear(slice_tt* pSlice)
{
    pSlice->szData  = NULL;
    pSlice->nLength = 0;
}

static inline void slice_set(slice_tt* pSlice, const char* szData /*=""*/, size_t n /*=0*/)
{
    pSlice->szData  = szData;
    pSlice->nLength = n;
}

static inline const char* slice_getData(slice_tt* pSlice)
{
    return pSlice->szData;
}

static inline size_t slice_getLength(slice_tt* pSlice)
{
    return pSlice->nLength;
}

static inline size_t slice_empty(slice_tt* pSlice)
{
    return pSlice->nLength == 0;
}

static inline int32_t slice_compare(slice_tt* pSlice, slice_tt* p)
{
    const size_t nMinLen = (pSlice->nLength < p->nLength) ? pSlice->nLength : p->nLength;
    int32_t      r       = memcmp(pSlice->szData, p->szData, nMinLen);
    if (r == 0) {
        if (pSlice->nLength < p->nLength) {
            r = -1;
        }
        else if (pSlice->nLength > p->nLength) {
            r = 1;
        }
    }
    return r;
}

static inline char* slice_toString(slice_tt* pSlice)
{
    if (pSlice->nLength > 0) {
        char* p = (char*)mem_malloc(pSlice->nLength);
        memcpy(p, pSlice->szData, pSlice->nLength);
        return p;
    }
    return NULL;
}

static inline void slice_removeSuffix(slice_tt* pSlice, size_t n)
{
    assert(n <= pSlice->nLength);
    pSlice->nLength -= n;
}

static inline void slice_readOffset(slice_tt* pSlice, size_t n)
{
    assert(n <= pSlice->nLength);
    pSlice->szData += n;
    pSlice->nLength -= n;
}

static inline bool slice_readBytes(slice_tt* pSlice, void* pOutBytes, size_t nMaxLengthToRead,
                                   bool bPeek /*= false*/)
{
    size_t nBytesToRead = pSlice->nLength < nMaxLengthToRead ? pSlice->nLength : nMaxLengthToRead;
    if (nBytesToRead == 0) {
        return false;
    }
    memcpy(pOutBytes, pSlice->szData, nBytesToRead);
    if (!bPeek) {
        pSlice->szData += nBytesToRead;
        pSlice->nLength -= nBytesToRead;
    }
    return true;
}
