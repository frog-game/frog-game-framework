set(CORE_LIBRARY "frogCore")

set(INSTALL_CORE_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/core" CACHE PATH "Installation directory for headers")
set(INSTALL_CORE_EVENTIO_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/core/eventIO" CACHE PATH "Installation directory for headers")
set(INSTALL_CORE_CXXMEM_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/core/cxx-mem" CACHE PATH "Installation directory for headers")
set(INSTALL_CORE_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_CORE_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for libraries")

set(CORE_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/macros_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/platform_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/utility_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/hash_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/time_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/thread_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/fs_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/queue_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rbtree_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/spinLock_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/rwSpinLock_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/heap_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/log_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/inetAddress_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/slice_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/cbuf_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/byteQueue_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/msgpack/msgpackEncode_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/msgpack/msgpackDecode_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/openssl/crypt_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/openssl/ssl_t.h
)

set(CORE_EVENTIO_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/eventIO/eventAsync_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/eventIO/eventIO_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/eventIO/eventIOThread_t.h
)

set(CORE_CXXMEM_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/cxx-mem/new_delete_override_t.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/cxx-mem/stl_allocator_t.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/cxx-mem/stl-override_t.hpp
)

set(CORE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/utility_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/time_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/fs_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/inetAddress_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/log_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/detail/byteQueue_t.c
)

if(WINDOWS)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/thread/win/thread_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/win/eventIO_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/win/eventWatcher_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/win/eventListenPort_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/win/eventConnection_t.c)
else()
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/thread/pthread/thread_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/eventIOLoop_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/eventIO_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/eventWatcher_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/eventListenPort_t.c)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/eventConnection_t.c)
endif()

if(LINUX)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/poller/poller_epoll_t.c)
elseif(ANDROID)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/poller/poller_select_t.c)
elseif(APPLE)
	list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/posix/poller/poller_kqueue_t.c)
endif()

list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/eventTimer_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/eventIO/eventIOThread_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/msgpack/msgpackEncode_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/msgpack/msgpackDecode_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/codec/tpack_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/codec/webSocket_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/openssl/crypt_t.c)
list(APPEND CORE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/openssl/ssl_t.c)

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${FROG_3RDPARTY_BINARY_DIR}/install/mimalloc/lib/mimalloc-1.6/include
	${FROG_3RDPARTY_BINARY_DIR}/install/openssl/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/mimalloc/lib/mimalloc-1.6)

if(IOS)
	set(CORE_LIBRARY_TYPE STATIC)
else()
	set(CORE_LIBRARY_TYPE SHARED)
endif()

add_library(${CORE_LIBRARY} ${CORE_LIBRARY_TYPE} ${CORE_SOURCE_FILES})

FUNCTION_COMPILE_DEFINE(${CORE_LIBRARY})
FUNCTION_COMPILE_OPTION(${CORE_LIBRARY})

if(BUILD_MIMALLOC)
	target_compile_definitions(${CORE_LIBRARY} PRIVATE _DEF_USE_MIMALLOC)
endif(BUILD_MIMALLOC)

target_compile_definitions(${CORE_LIBRARY} PRIVATE _DEF_CORE_DLLEXPORT)

set_target_properties(${CORE_LIBRARY} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${CORE_LIBRARY} PROPERTIES C_STANDARD 11)

if(LINUX)
	set_target_properties(${CORE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${CORE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(ANDROID)
	set_target_properties(${CORE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${CORE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(APPLE)
	set_target_properties(${CORE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${CORE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
endif()

set_target_properties(${CORE_LIBRARY} PROPERTIES INSTALL_RPATH "${INSTALL_CORE_BIN_DIR}")

if(WINDOWS)
	target_link_libraries(${CORE_LIBRARY} PRIVATE OpenSSL::SSL OpenSSL::Crypto Crypt32.lib Ws2_32.lib
	)

	if(BUILD_MIMALLOC)
		target_link_libraries(${CORE_LIBRARY} PRIVATE mimalloc-static)
	endif(BUILD_MIMALLOC)
else()
	target_link_libraries(${CORE_LIBRARY} PRIVATE OpenSSL::SSL OpenSSL::Crypto ${CMAKE_THREAD_LIBS_INIT}
	)

	if(BUILD_MIMALLOC)
		target_link_libraries(${CORE_LIBRARY} PRIVATE mimalloc)
	endif(BUILD_MIMALLOC)
endif()

install(FILES ${CORE_HEADER_FILES} DESTINATION "${INSTALL_CORE_INCLUDE_DIR}")
install(FILES ${CORE_EVENTIO_HEADER_FILES} DESTINATION "${INSTALL_CORE_EVENTIO_INCLUDE_DIR}")
install(FILES ${CORE_CXXMEM_HEADER_FILES} DESTINATION "${INSTALL_CORE_CXXMEM_INCLUDE_DIR}")
install(TARGETS ${CORE_LIBRARY} DESTINATION "${INSTALL_CORE_LIB_DIR}")
install(TARGETS ${CORE_LIBRARY} LIBRARY DESTINATION "${INSTALL_CORE_BIN_DIR}")
