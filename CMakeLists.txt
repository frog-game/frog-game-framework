cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)

# Frog version
set(FROG_MAJOR_VERSION 0)
set(FROG_MINOR_VERSION 1)
set(FROG_PATCH_VERSION 0)

# package set
set(PACKAGE_NAME "Frog")
set(PACKAGE_VERSION_MAJOR ${FROG_MAJOR_VERSION})
set(PACKAGE_VERSION_MINOR ${FROG_MINOR_VERSION})
set(PACKAGE_VERSION_PATCH ${FROG_PATCH_VERSION})
set(PACKAGE_VERSION ${FROG_MAJOR_VERSION}.${FROG_MINOR_VERSION}.${FROG_PATCH_VERSION})
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME "${PACKAGE_NAME}-v${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/Frog/Frog/issues")

# Frog
project(${PACKAGE_NAME})

# root
set(FROG_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${FROG_ROOT_PATH}/cmake/modules/)

set(FROG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(FROG_3RDPARTY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
set(FROG_3RDPARTY_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/3rdparty)

option(MSVC_USE_STATIC_RUNTIME_LIBRARY "msvc use static runitme library" OFF)

include(build_config)

# build options
option(BUILD_OPENSSL "build openssl" OFF)
option(BUILD_MIMALLOC "build mimalloc" ON)
option(BUILD_LPEG "build lpeg" ON)
option(ENABLE_UNITTEST "enable unittest" ON)
option(ENABLE_BENCHMARK "enable benchmark" ON)
option(DEBUG_3RDPARTY_OUTPUT "debug 3rdparty output" OFF)
option(INSTALL_3RDPARTY "install 3rdparty" ON)

# install 3rdparty
if(INSTALL_3RDPARTY)
    include(install_3rdparty)
endif()

if(BUILD_OPENSSL)
    set(OPENSSL_ROOT_DIR ${FROG_3RDPARTY_BINARY_DIR}/install/openssl)
endif()

find_package(OpenSSL REQUIRED)

add_subdirectory(
    src
)
