#pragma once

#ifdef __cplusplus

extern "C" {
#    include <utility_t.h>
}

#    if (__cplusplus >= 201703)
#        define _decl_nodiscard [[nodiscard]]
#    elif (__GNUC__ >= 4) || defined(__clang__)   // includes clang, icc, and clang-cl
#        define _decl_nodiscard __attribute__((warn_unused_result))
#    elif (_MSC_VER >= 1700)
#        define _decl_nodiscard _Check_return_
#    else
#        define _decl_nodiscard
#    endif

#    if (__cplusplus >= 201103L) || (_MSC_VER > 1900)   // C++11
#        define _attr_noexcept noexcept
#    else
#        define _attr_noexcept throw()
#    endif

#    include <cstdint>                                  // PTRDIFF_MAX
#    if (__cplusplus >= 201103L) || (_MSC_VER > 1900)   // C++11
#        include <type_traits>                          // std::true_type
#        include <utility>                              // std::forward
#    endif

template<class T> struct mem_stl_allocator
{
    typedef T                 value_type;
    typedef std::size_t       size_type;
    typedef std::ptrdiff_t    difference_type;
    typedef value_type&       reference;
    typedef value_type const& const_reference;
    typedef value_type*       pointer;
    typedef value_type const* const_pointer;
    template<class U> struct rebind
    {
        typedef mem_stl_allocator<U> other;
    };

    mem_stl_allocator() _attr_noexcept                         = default;
    mem_stl_allocator(const mem_stl_allocator&) _attr_noexcept = default;
    template<class U> mem_stl_allocator(const mem_stl_allocator<U>&) _attr_noexcept {}
    mem_stl_allocator select_on_container_copy_construction() const { return *this; }
    void              deallocate(T* p, size_type) { mem_free(p); }

#    if (__cplusplus >= 201703L)   // C++17
    _decl_nodiscard T* allocate(size_type count)
    {
        return static_cast<T*>(mem_malloc(count * sizeof(T)));
    }
    _decl_nodiscard T* allocate(size_type count, const void*) { return allocate(count); }
#    else
    _decl_nodiscard pointer allocate(size_type count, const void* = 0)
    {
        return static_cast<pointer>(mem_malloc(count * sizeof(value_type)));
    }
#    endif

#    if ((__cplusplus >= 201103L) || (_MSC_VER > 1900))   // C++11
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;
    using is_always_equal                        = std::true_type;
    template<class U, class... Args> void construct(U* p, Args&&... args)
    {
        ::new (p) U(std::forward<Args>(args)...);
    }
    template<class U> void destroy(U* p) _attr_noexcept { p->~U(); }
#    else
    void construct(pointer p, value_type const& val) { ::new (p) value_type(val); }
    void destroy(pointer p) { p->~value_type(); }
#    endif

    size_type     max_size() const _attr_noexcept { return (PTRDIFF_MAX / sizeof(value_type)); }
    pointer       address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }
};

template<class T1, class T2>
bool operator==(const mem_stl_allocator<T1>&, const mem_stl_allocator<T2>&) _attr_noexcept
{
    return true;
}
template<class T1, class T2>
bool operator!=(const mem_stl_allocator<T1>&, const mem_stl_allocator<T2>&) _attr_noexcept
{
    return false;
}

#endif   // __cplusplus