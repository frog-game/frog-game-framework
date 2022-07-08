#pragma once

#define DEF_PLATFORM_UNKNOWN 0
#define DEF_PLATFORM_WINDOWS 1
#define DEF_PLATFORM_LINUX 2
#define DEF_PLATFORM_MACOS 3
#define DEF_PLATFORM_ANDROID 4
#define DEF_PLATFORM_IOS 5

#if defined(_WINDOWS) || defined(_WIN32)
#    define DEF_PLATFORM DEF_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#    if __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 30000 || \
        __IPHONE_OS_VERSION_MIN_REQUIRED > 30000
#        define DEF_PLATFORM DEF_PLATFORM_IOS
#    else
#        define DEF_PLATFORM DEF_PLATFORM_MACOS
#    endif
#elif defined(__ANDROID__)
#    define DEF_PLATFORM DEF_PLATFORM_ANDROID
#elif defined(__linux__)
#    define DEF_PLATFORM DEF_PLATFORM_LINUX
#else
#    define DEF_PLATFORM DEF_PLATFORM_UNKNOWN
#endif

#if DEF_PLATFORM == DEF_PLATFORM_WINDOWS
#    if defined(_WIN64)
#        define DEF_PLATFORM_64BITS
#    endif
#elif DEF_PLATFORM == DEF_PLATFORM_ANDROID
#    if __LP64__
#        define DEF_PLATFORM_64BITS
#    endif
#elif DEF_PLATFORM == DEF_PLATFORM_IOS
#    if __LP64__
#        define DEF_PLATFORM_64BITS
#    endif
#elif DEF_PLATFORM == DEF_PLATFORM_MACOS
#    define DEF_PLATFORM_64BITS
#elif DEF_PLATFORM == DEF_PLATFORM_LINUX
#    if defined(_LINUX64) || defined(_LP64)
#        define DEF_PLATFORM_64BITS
#    endif
#endif

#if DEF_PLATFORM == DEF_PLATFORM_WINDOWS
#    ifdef _DEF_CORE_DLLEXPORT
#        define frCore_API __declspec(dllexport)
#        define frCore_DEF __declspec(dllexport) extern
#    else
#        define frCore_API __declspec(dllimport)
#        define frCore_DEF __declspec(dllimport) extern
#    endif
#else
#    ifdef _DEF_CORE_DLLEXPORT
#        define frCore_API __attribute__((__visibility__("default")))
#        define frCore_DEF __attribute__((__visibility__("default"))) extern
#    else
#        define frCore_API extern
#        define frCore_DEF extern
#    endif
#endif