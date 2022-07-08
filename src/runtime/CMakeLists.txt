set(RUNTIME_LIBRARY "lruntime")

set(INSTALL_RUNTIME_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for modules")

set(RUNTIME_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/crypt/lcrypt_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/buffer/lbuffer_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/msgpack/lmsgpack_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/stream/ltpack_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/stream/lwebSocket_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/http/lhttp_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/http/lhttpTransfer_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/db/lmysql_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/db/lredis_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/channel/lchannelExt_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/debug/ldebug_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/internal/lpackagePath_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/internal/lloadCache_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/internal/lconfig_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/service/lservice_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/sharetable/lsharetable_t.h
)

set(RUNTIME_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/crypt/lcrypt_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/buffer/lbuffer_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/msgpack/lmsgpack_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/stream/ltpack_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/stream/lwebSocket_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/http/lhttp_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/http/lhttpTransfer_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/db/lmysql_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/db/lredis_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/channel/lchannelExt_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/debug/ldebug_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/lpackagePath_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/lloadCache_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/lconfig_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/lconnector_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/llistenPort_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/ldnsResolve_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/internal/ltimerWatcher_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/service/lservice_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/env/lenv_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/sharetable/lsharetable_t.c
)

include_directories(
	${FROG_3RDPARTY_BINARY_DIR}/install/lua/include
	${FROG_3RDPARTY_BINARY_DIR}/install/http-parser/include
	${FROG_3RDPARTY_BINARY_DIR}/install/multipart-parser/include
	${FROG_3RDPARTY_BINARY_DIR}/install/mysql-parser/include
	${FROG_3RDPARTY_BINARY_DIR}/install/redis-parser/include
	${FROG_SOURCE_DIR}/core/include
	${FROG_SOURCE_DIR}/service/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/lua/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/http-parser/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/multipart-parser/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/mysql-parser/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/redis-parser/lib)

if(IOS)
	set(RUNTIME_LIBRARY_TYPE STATIC)
else()
	set(RUNTIME_LIBRARY_TYPE SHARED)
endif()

add_library(${RUNTIME_LIBRARY} ${RUNTIME_LIBRARY_TYPE} ${RUNTIME_HEADER_FILES} ${RUNTIME_SOURCE_FILES})

FUNCTION_COMPILE_DEFINE(${RUNTIME_LIBRARY})
FUNCTION_COMPILE_OPTION(${RUNTIME_LIBRARY})

set_target_properties(${RUNTIME_LIBRARY} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${RUNTIME_LIBRARY} PROPERTIES C_STANDARD 11)
set_target_properties(${RUNTIME_LIBRARY} PROPERTIES PREFIX "")

if(LINUX)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(ANDROID)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(APPLE)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${RUNTIME_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
endif()

set_target_properties(${RUNTIME_LIBRARY} PROPERTIES INSTALL_RPATH "${INSTALL_RUNTIME_DIR}")

if(WINDOWS)
	target_link_libraries(${RUNTIME_LIBRARY} PRIVATE Ws2_32.lib
	)
else()
	target_link_libraries(${RUNTIME_LIBRARY} PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	)
endif()

target_link_libraries(${RUNTIME_LIBRARY} PRIVATE frogService frogCore lua http-parser multipart-parser mysql-parser redis-parser)

install(TARGETS ${RUNTIME_LIBRARY} DESTINATION "${INSTALL_RUNTIME_DIR}/modules")
