#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#include "platform_t.h"

#define container_of(ptr, type, member) ((type*)((char*)(ptr)-offsetof(type, member)))

#define align_size(n, alignment) ((n + alignment - 1) & ~(alignment - 1))

#ifdef _MSC_VER
#    define _decl_forceInline __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#    define _decl_forceInline inline __attribute__((__always_inline__))
#else
#    define _decl_forceInline inline
#endif

#ifndef __UNUSED
#    if defined(__clang__) || defined(__GNUC__)
#        define __UNUSED __attribute__((unused))
#    elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#        define __UNUSED _Check_return_
#    else
#        define __UNUSED
#    endif
#endif

#ifdef _MSC_VER
#    define _decl_noInline __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#    define _decl_noInline __attribute__((__noinline__))
#else
#    define _decl_noInline
#endif

#ifdef _MSC_VER
#    define _decl_threadLocal __declspec(thread)
#else
#    define _decl_threadLocal __thread
#endif

#define CPU_CACHE_LINE 64

#if defined(_MSC_VER)
#    define _decl_cpu_cache_align __declspec(align(CPU_CACHE_LINE))
#elif defined(__clang__) || defined(__GNUC__)
#    define _decl_cpu_cache_align __attribute__((aligned(CPU_CACHE_LINE)))
#else
#    define _decl_cpu_cache_align
#endif

#if defined(__GNUC__)
#    define _Likely(x) (__builtin_expect((x), 1))
#    define _UnLikely(x) (__builtin_expect((x), 0))
#else
#    define _Likely(x) (x)
#    define _UnLikely(x) (x)
#endif

#if defined(_WINDOWS) || defined(_WIN32)
#    define bzero(s, n) memset(s, 0, n)
#    define strcasecmp _stricmp
#    define strncasecmp _strnicmp
#    define strtok_r strtok_s
#endif

typedef struct ioBufVec_s
{
    char*   pBuf;
    int32_t iLength;
} ioBufVec_tt;

static inline size_t strlncat(char* dst, size_t len, const char* src, size_t n)
{
    size_t slen;
    size_t dlen;
    size_t rlen;
    size_t ncpy;

    slen = strnlen(src, n);
    dlen = strnlen(dst, len);

    if (slen + dlen >= len) {
        return 0;
    }

    if (dlen < len) {
        rlen = len - dlen;
        ncpy = slen < rlen ? slen : (rlen - 1);
        memcpy(dst + dlen, src, ncpy);
        dst[dlen + ncpy] = '\0';
    }

    return slen + dlen;
}


frCore_DEF void* (*mem_malloc)(size_t size);
frCore_DEF void* (*mem_realloc)(void* ptr, size_t size);
frCore_DEF void (*mem_free)(void* ptr);

frCore_API void set_mem_functions(void* (*fnMalloc)(size_t), void* (*fnRealloc)(void*, size_t),
                                  void (*fnFree)(void*));

frCore_API char* mem_strdup(const char* s);
frCore_API char* mem_strndup(const char* s, size_t n);

frCore_API char* str_replace_all(char* str, char* sub, char* replace);

frCore_API char** str_split(const char* s, const char* delim);
frCore_API char** str_split_count(const char* s, const char* del, size_t* nb);