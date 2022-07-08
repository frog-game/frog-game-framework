#include "utility_t.h"

#ifdef _DEF_USE_MIMALLOC
#    include "mimalloc.h"
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _DEF_USE_MIMALLOC
void* (*mem_malloc)(size_t size)             = mi_malloc;
void* (*mem_realloc)(void* ptr, size_t size) = mi_realloc;
void (*mem_free)(void* ptr)                  = mi_free;
#else
#    if defined(_WINDOWS) || defined(_WIN32)
static inline void* default_malloc(size_t size)
{
    return malloc(size);
}
static inline void* default_realloc(void* p, size_t size)
{
    return realloc(p, size);
}
static inline void default_free(void* p)
{
    free(p);
}
#    else
#        define default_malloc malloc
#        define default_realloc realloc
#        define default_free free
#    endif
void* (*mem_malloc)(size_t size)             = default_malloc;
void* (*mem_realloc)(void* ptr, size_t size) = default_realloc;
void (*mem_free)(void* ptr)                  = default_free;
#endif

void set_mem_functions(void* (*fnMalloc)(size_t), void* (*fnRealloc)(void*, size_t),
                       void (*fnFree)(void*))
{
    if (fnMalloc) {
        mem_malloc = fnMalloc;
    }
    if (fnRealloc) {
        mem_realloc = fnRealloc;
    }
    if (fnFree) {
        mem_free = fnFree;
    }
}

char* mem_strdup(const char* s)
{
#ifdef _DEF_USE_MIMALLOC
    return mi_strdup(s);
#else
    if (s == NULL) {
                         return NULL;
    }
    size_t n   = strlen(s);
    char*  buf = mem_malloc(n + 1);
    if (buf == NULL) {
                         return NULL;
    }
    memcpy(buf, s, n + 1);
    return buf;
#endif
}

char* mem_strndup(const char* s, size_t n)
{
#ifdef _DEF_USE_MIMALLOC
    return mi_strndup(s, n);
#else
    if (s == NULL) return NULL;
    const char*  end = (const char*)memchr(s, 0, n);
    const size_t m   = (end != NULL ? (size_t)(end - s) : n);
    assert(m <= n);
    char* buf = mem_malloc(m + 1);
    if (buf == NULL) {
                         return NULL;
    }
    memcpy(buf, s, m);
    buf[m] = '\0';
    return buf;
#endif
}

char* str_replace_all(char* str, char* sub, char* replace)
{
    char bstr[strlen(str)];   //转换缓冲区
    memset(bstr, 0, sizeof(bstr));

    for (int i = 0; i < strlen(str); i++) {

        if (!strncmp(str + i, sub, strlen(sub))) {   //查找目标字符串

            strcat(bstr, replace);

            i += strlen(sub) - 1;
        }
        else {

            strncat(bstr, str + i, 1);   //保存一字节进缓冲区
        }
    }

    strcpy(str, bstr);

    return str;
}


static char** _strsplit(const char* s, const char* delim, size_t* nb)
{
    void*        data;
    char*        _s = (char*)s;
    const char** ptrs;
    size_t       ptrsSize, nbWords = 1, sLen = strlen(s), delimLen = strlen(delim);

    while ((_s = strstr(_s, delim))) {
        _s += delimLen;
        ++nbWords;
    }
    ptrsSize = (nbWords + 1) * sizeof(char*);
    ptrs = data = malloc(ptrsSize + sLen + 1);
    if (data) {
        *ptrs = _s = strcpy(((char*)data) + ptrsSize, s);
        if (nbWords > 1) {
            while ((_s = strstr(_s, delim))) {
                *_s = '\0';
                _s += delimLen;
                *++ptrs = _s;
            }
        }
        *++ptrs = NULL;
    }
    if (nb) {
        *nb = data ? nbWords : 0;
    }
    return data;
}

char** str_split(const char* s, const char* delim)
{
    return _strsplit(s, delim, NULL);
}

char** str_split_count(const char* s, const char* delim, size_t* nb)
{
    return _strsplit(s, delim, nb);
}