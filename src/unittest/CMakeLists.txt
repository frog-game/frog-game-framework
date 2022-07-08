set(UNITTEST_EXE "unittest")

set(INSTALL_UNITTEST_EXE_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")

set(UNITTEST_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_byteQueue.cc
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_inetAddress.cc
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_thread.cc
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_thread2.cc
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_time.cc
	${CMAKE_CURRENT_SOURCE_DIR}/source/test_eventIO.cc
)

include_directories(
	${FROG_3RDPARTY_BINARY_DIR}/install/googletest/include
	${FROG_3RDPARTY_BINARY_DIR}/install/lua/include
	${FROG_SOURCE_DIR}/core/include
	${FROG_SOURCE_DIR}/service/include
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/googletest/lib)
link_directories(${FROG_3RDPARTY_BINARY_DIR}/install/lua/lib)

add_executable(${UNITTEST_EXE}
	${UNITTEST_SOURCE_FILES}
)

FUNCTION_COMPILE_DEFINE(${UNITTEST_EXE})
FUNCTION_COMPILE_OPTION(${UNITTEST_EXE})

target_compile_definitions(${UNITTEST_EXE} PUBLIC _GOOGLE_TEST)

set_target_properties(${UNITTEST_EXE} PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(${UNITTEST_EXE} PROPERTIES CXX_STANDARD 11)

if(WINDOWS)
	if(DEBUG_3RDPARTY_OUTPUT)
		set(gTestlib gtest_maind gtestd)
	else()
		set(gTestlib gtest_main gtest)
	endif()

	target_link_libraries(${UNITTEST_EXE} PRIVATE ${gTestlib} frogService frogCore lua Ws2_32.lib
	)
else()
	target_link_libraries(${UNITTEST_EXE} PRIVATE gtest_main gtest frogService frogCore lua ${CMAKE_THREAD_LIBS_INIT} dl
	)
endif()

set_target_properties(${UNITTEST_EXE} PROPERTIES INSTALL_RPATH "${INSTALL_UNITTEST_EXE_DIR}")

install(TARGETS ${UNITTEST_EXE} DESTINATION "${INSTALL_UNITTEST_EXE_DIR}")
