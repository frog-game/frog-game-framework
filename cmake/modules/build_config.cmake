if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "set the configurations" FORCE)
    message(STATUS "CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
endif()    

# check c standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#build system
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(APPLE TRUE)
    if(NOT IOS)
        set(MACOS TRUE)  
    endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if(NOT ANDROID)
        set(LINUX TRUE)
    endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(ANDROID TRUE)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")  
    set(WINDOWS TRUE)
else()
    message(FATAL_ERROR  "unknown system" )
    return()
endif()

message(STATUS "BUILD SYSTEM: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")

#thread
if(LINUX)
    find_package(Threads REQUIRED)
endif(LINUX)

if(MSVC)
    if(MSVC_USE_STATIC_RUNTIME_LIBRARY)
        set(CMAKE_USER_MAKE_RULES_OVERRIDE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc/compiler_c_flags_overrides.cmake)
        set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc/compiler_cxx_flags_overrides.cmake)
    endif()
elseif(LINUX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -fpic")
endif()

 # compiler function 
function(FUNCTION_COMPILE_DEFINE target)
    if(APPLE)
        target_compile_definitions(${target} 
            PUBLIC __APPLE__
        )
    elseif(LINUX)
        target_compile_definitions(${target} 
            PUBLIC __linux__
        )
    elseif(ANDROID)
        target_compile_definitions(${target} 
            PUBLIC __ANDROID__
        )
    elseif(WINDOWS)
        target_compile_definitions(${target} 
            PUBLIC WIN32
            PUBLIC _WIN32
            PUBLIC _WINDOWS
            PUBLIC _CRT_SECURE_NO_WARNINGS
            PUBLIC _SCL_SECURE_NO_WARNINGS
            PUBLIC _WINSOCK_DEPRECATED_NO_WARNINGS
            PUBLIC WIN32_LEAN_AND_MEAN
            PUBLIC LUA_BUILD_AS_DLL
        )
    endif()
endfunction()

function(FUNCTION_COMPILE_OPTION target)
    if(MSVC AND MSVC_USE_STATIC_RUNTIME_LIBRARY)
        target_compile_options(${target}
            PUBLIC "/MT$<$<STREQUAL:$<CONFIGURATION>,Debug>:d>"
        )
    endif()
 endfunction()
