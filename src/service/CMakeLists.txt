set(SERVICE_LIBRARY "frogService")

set(INSTALL_SERVICE_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/service" CACHE PATH "Installation directory for headers")
set(INSTALL_SERVICE_CHANNEL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/service/channel" CACHE PATH "Installation directory for headers")
set(INSTALL_SERVICE_STREAM_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/service/stream" CACHE PATH "Installation directory for headers")
set(INSTALL_SERVICE_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_SERVICE_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for libraries")

set(SERVICE_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/serviceEvent_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/service_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/serviceCenter_t.h
)

set(SERVICE_CHANNEL_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/channel/channelCenter_t.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/channel/channel_t.h
)

set(SERVICE_STREAM_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/include/stream/codecStream_t.h
)

set(SERVICE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/serviceCenter_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/serviceMonitor_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/service_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/connector_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/listenPort_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/timerWatcher_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/dnsResolve_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/channel/channelCenter_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/channel/channel_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/stream/tpackStream_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/stream/webSocketStream_t.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/stream/mysqlStream_t.c
)

include_directories(
	${FROG_3RDPARTY_BINARY_DIR}/install/cares/include
	${FROG_SOURCE_DIR}/core/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/cares/lib)

if(IOS)
	set(SERVICE_LIBRARY_TYPE STATIC)
else()
	set(SERVICE_LIBRARY_TYPE SHARED)
endif()

add_library(${SERVICE_LIBRARY} ${SERVICE_LIBRARY_TYPE} ${SERVICE_SOURCE_FILES})

FUNCTION_COMPILE_DEFINE(${SERVICE_LIBRARY})
FUNCTION_COMPILE_OPTION(${SERVICE_LIBRARY})

target_compile_definitions(${SERVICE_LIBRARY} PRIVATE _DEF_SERVICE_DLLEXPORT)

set_target_properties(${SERVICE_LIBRARY} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${SERVICE_LIBRARY} PROPERTIES C_STANDARD 11)

if(LINUX)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(ANDROID)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
elseif(APPLE)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES C_VISIBILITY_PRESET hidden)
	set_target_properties(${SERVICE_LIBRARY} PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
endif()

set_target_properties(${SERVICE_LIBRARY} PROPERTIES INSTALL_RPATH "${INSTALL_SERVICE_BIN_DIR}")

if(WINDOWS)
	target_link_libraries(${SERVICE_LIBRARY} PRIVATE Ws2_32.lib
	)
else()
	target_link_libraries(${SERVICE_LIBRARY} PRIVATE ${CMAKE_THREAD_LIBS_INIT}
	)
endif()

target_link_libraries(${SERVICE_LIBRARY} PRIVATE frogCore cares)

install(FILES ${SERVICE_HEADER_FILES} DESTINATION "${INSTALL_SERVICE_INCLUDE_DIR}")
install(FILES ${SERVICE_CHANNEL_HEADER_FILES} DESTINATION "${INSTALL_SERVICE_CHANNEL_INCLUDE_DIR}")
install(FILES ${SERVICE_STREAM_HEADER_FILES} DESTINATION "${INSTALL_SERVICE_STREAM_INCLUDE_DIR}")
install(TARGETS ${SERVICE_LIBRARY} DESTINATION "${INSTALL_SERVICE_LIB_DIR}")
install(TARGETS ${SERVICE_LIBRARY} LIBRARY DESTINATION "${INSTALL_SERVICE_BIN_DIR}")