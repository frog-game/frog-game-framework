set(3RDPARTY_BUILD_COMMAND -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} )

if(ANDROID)
    set(3RDPARTY_BUILD_COMMAND ${3RDPARTY_BUILD_COMMAND}
        -DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_NDK=${ANDROID_NDK})
endif()

if(IOS)
    set(3RDPARTY_BUILD_COMMAND ${3RDPARTY_BUILD_COMMAND}
        -DIOS_PLATFORM=${IOS_PLATFORM}
        )
endif()


include(3rdparty/build_lua)
include(3rdparty/build_cares)

include(3rdparty/build_http_parser)
include(3rdparty/build_multipart_parser)
include(3rdparty/build_mysql_parser)
include(3rdparty/build_redis_parser)

if(BUILD_OPENSSL)
    include(3rdparty/build_openssl)
endif()

if(BUILD_MIMALLOC)
    include(3rdparty/build_mimalloc)
endif()

if(BUILD_LPEG)
    include(3rdparty/build_lpeg)
endif()

if(ENABLE_UNITTEST)
    include(3rdparty/build_googletest)
endif()

if(ENABLE_BENCHMARK)
    include(3rdparty/build_benchmark)
endif()
