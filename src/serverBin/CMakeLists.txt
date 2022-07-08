set(SERVER_BIN "serverBin")

set(INSTALL_SERVER_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")

set(SERVER_BIN_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/main.c
)

include_directories(
	${FROG_3RDPARTY_BINARY_DIR}/install/lua/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/lua/lib)

add_executable(${SERVER_BIN}
	${SERVER_BIN_SOURCE_FILES}
)

FUNCTION_COMPILE_DEFINE(${SERVER_BIN})
FUNCTION_COMPILE_OPTION(${SERVER_BIN})

set_target_properties(${SERVER_BIN} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${SERVER_BIN} PROPERTIES C_STANDARD 11)

if(WINDOWS)
	target_link_libraries(${SERVER_BIN} PRIVATE lua Ws2_32.lib
	)
else()
	target_link_libraries(${SERVER_BIN} PRIVATE lua ${CMAKE_THREAD_LIBS_INIT} dl m
	)
endif()

set_target_properties(${SERVER_BIN} PROPERTIES INSTALL_RPATH "${INSTALL_SERVER_BIN_DIR}")

install(TARGETS ${SERVER_BIN} DESTINATION "${INSTALL_SERVER_BIN_DIR}")
