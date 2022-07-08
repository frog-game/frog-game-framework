#pragma once

#include "stl_allocator_t.hpp"

#ifdef __cplusplus
#    include <memory>
#    include <algorithm>
#    include <utility>
#    include <limits>
#    include <tuple>
#    include <initializer_list>
#    include <string>
#    include <forward_list>
#    include <array>
#    include <list>
#    include <vector>
#    include <deque>
#    include <queue>
#    include <stack>
#    include <set>
#    include <map>
#    include <unordered_map>
#    include <unordered_set>
#    include <functional>
#    include <type_traits>

template<class _Ty> using forward_list = typename std::forward_list<_Ty, mem_stl_allocator<_Ty>>;

template<class _Ty> using vector = typename std::vector<_Ty, mem_stl_allocator<_Ty>>;

template<class _Ty> using list = typename std::list<_Ty, mem_stl_allocator<_Ty>>;

template<class _Ty> using deque = typename std::deque<_Ty, mem_stl_allocator<_Ty>>;

template<class _Ty> using queue = typename std::queue<_Ty, deque<_Ty>>;

template<class _Ty> using stack = typename std::stack<_Ty, deque<_Ty>>;

template<class _Kty, class _Ty, class _Pr = std::less<_Kty>>
using map = typename std::map<_Kty, _Ty, _Pr, mem_stl_allocator<std::pair<const _Kty, _Ty>>>;

template<class _Kty, class _Ty, class _Pr = std::less<_Kty>>
using multimap =
    typename std::multimap<_Kty, _Ty, _Pr, mem_stl_allocator<std::pair<const _Kty, _Ty>>>;

template<class _Kty, class _Pr = std::less<_Kty>>
using set = typename std::set<_Kty, _Pr, mem_stl_allocator<_Kty>>;

template<class _Kty, class _Pr = std::less<_Kty>>
using multiset = typename std::multiset<_Kty, _Pr, mem_stl_allocator<_Kty>>;

template<class _Kty, class _Ty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>>
using unordered_map = typename std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq,
                                                  mem_stl_allocator<std::pair<const _Kty, _Ty>>>;

template<class _Kty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>>
using unordered_set = typename std::unordered_set<_Kty, _Hasher, _Keyeq, mem_stl_allocator<_Kty>>;

typedef std::basic_string<char, std::char_traits<char>, mem_stl_allocator<char>> string;

typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, mem_stl_allocator<wchar_t>> wstring;

typedef std::basic_ostringstream<char, std::char_traits<char>, mem_stl_allocator<char>>
    ostringstream;

typedef std::basic_stringstream<char, std::char_traits<char>, mem_stl_allocator<char>> stringstream;

#endif   // __cplusplus