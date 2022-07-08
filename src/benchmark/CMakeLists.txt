set(BENCHMARKTEST_EXE "benchmarktest")

set(INSTALL_BENCHMARKTEST_EXE_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")

set(BENCHMARKTEST_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/spin_lock/spinLock.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/spin_lock/mscLock.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/spin_lock/clhLock.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/spin_lock/rwSpinLock.c
	${CMAKE_CURRENT_SOURCE_DIR}/source/threadLock_benchmark.cc
)

include_directories(
	${FROG_3RDPARTY_BINARY_DIR}/install/benchmark/include
	${FROG_3RDPARTY_BINARY_DIR}/install/lua/include
	${FROG_SOURCE_DIR}/core/include
	${FROG_SOURCE_DIR}/service/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/benchmark/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/lua/lib)

add_executable(${BENCHMARKTEST_EXE}
	${BENCHMARKTEST_SOURCE_FILES}
)

FUNCTION_COMPILE_DEFINE(${BENCHMARKTEST_EXE})
FUNCTION_COMPILE_OPTION(${BENCHMARKTEST_EXE})

set_target_properties(${BENCHMARKTEST_EXE} PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(${BENCHMARKTEST_EXE} PROPERTIES CXX_STANDARD 11)

if(WINDOWS)
	target_link_libraries(${BENCHMARKTEST_EXE} PRIVATE benchmark_main benchmark frogService frogCore lua Ws2_32.lib Shlwapi.lib
	)
else()
	target_link_libraries(${BENCHMARKTEST_EXE} PRIVATE benchmark_main benchmark frogService frogCore lua ${CMAKE_THREAD_LIBS_INIT} dl
	)
endif()

set_target_properties(${BENCHMARKTEST_EXE} PROPERTIES INSTALL_RPATH "${INSTALL_BENCHMARKTEST_EXE_DIR}")

install(TARGETS ${BENCHMARKTEST_EXE} DESTINATION "${INSTALL_BENCHMARKTEST_EXE_DIR}")
