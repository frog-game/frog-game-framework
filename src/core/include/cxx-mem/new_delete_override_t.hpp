#pragma once

#ifdef __cplusplus
#    include <new>

extern "C" {
#    include <utility_t.h>
}

void operator delete(void* p) noexcept
{
    mem_free(p);
};
void operator delete[](void* p) noexcept
{
    mem_free(p);
};

void* operator new(std::size_t n) noexcept(false)
{
    return mem_malloc(n);
}
void* operator new[](std::size_t n) noexcept(false)
{
    return mem_malloc(n);
}

void* operator new(std::size_t n, const std::nothrow_t& tag) noexcept
{
    (void)(tag);
    return mem_malloc(n);
}
void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept
{
    (void)(tag);
    return mem_malloc(n);
}

#    if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept
{
    mem_free(p);
};
void operator delete[](void* p, std::size_t n) noexcept
{
    mem_free(p);
};
#    endif

#endif   // __cplusplus
