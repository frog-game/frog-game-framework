set(BUILD_3RDPARTY_NAME "openssl")
set(BUILD_3RDPARTY_FILENAME "openssl-OpenSSL_1_1_1g.tar.gz")

message(STATUS "BUILD NAME:(openssl)")

get_filename_component(filename "${FROG_ROOT_PATH}/dependent/${BUILD_3RDPARTY_FILENAME}" ABSOLUTE)
get_filename_component(directory "${FROG_3RDPARTY_BINARY_DIR}/source/${BUILD_3RDPARTY_NAME}" ABSOLUTE)

message(STATUS "extracting...
		 src='${filename}'
		 dst='${directory}'")

if(NOT EXISTS "${filename}")
	message(FATAL_ERROR "error: file to extract does not exist: '${filename}'")
endif()

# Prepare a space for extracting:
#
set(i 1234)

while(EXISTS "${directory}/../ex-${BUILD_3RDPARTY_NAME}${i}")
	math(EXPR i "${i} + 1")
endwhile()

set(ut_dir "${directory}/../ex-${BUILD_3RDPARTY_NAME}${i}")
file(MAKE_DIRECTORY "${ut_dir}")

# Extract it:
#
message(STATUS "extracting... [tar xfz]")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz ${filename}
	WORKING_DIRECTORY ${ut_dir}
	RESULT_VARIABLE rv)

if(NOT rv EQUAL 0)
	message(STATUS "extracting... [error clean up]")
	file(REMOVE_RECURSE "${ut_dir}")
	message(FATAL_ERROR "error: extract of '${filename}' failed")
endif()

# Analyze what came out of the tar file:
#
message(STATUS "extracting... [analysis]")
file(GLOB contents "${ut_dir}/*")
list(REMOVE_ITEM contents "${ut_dir}/.DS_Store")
list(LENGTH contents n)

if(NOT n EQUAL 1 OR NOT IS_DIRECTORY "${contents}")
	set(contents "${ut_dir}")
endif()

# Move "the one" directory to the final directory:
#
message(STATUS "extracting... [rename]")
file(REMOVE_RECURSE ${directory})
get_filename_component(contents ${contents} ABSOLUTE)
file(RENAME ${contents} ${directory})

# Clean up:
#
message(STATUS "extracting... [clean up]")
file(REMOVE_RECURSE "${ut_dir}")

message(STATUS "extracting... done")
file(MAKE_DIRECTORY ${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME})

if(DEBUG_3RDPARTY_OUTPUT)
	if(WINDOWS)
		if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR(CMAKE_GENERATOR MATCHES "Win64"))
			execute_process(COMMAND perl Configure VC-WIN64A no-shared --debug --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		else()
			execute_process(COMMAND perl Configure VC-WIN32 no-asm no-shared --debug --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		endif()

		execute_process(COMMAND nmake install WORKING_DIRECTORY ${directory})
	elseif(MACOS)
		execute_process(COMMAND ./Configure darwin64-x86_64-cc -fPIC no-shared --debug --prefix=${
			
		}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make install WORKING_DIRECTORY ${directory})
	elseif(LINUX)
		execute_process(COMMAND ./config -fPIC no-shared no-tests --debug --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make install WORKING_DIRECTORY ${directory})
	endif()
else()
	if(WINDOWS)
		if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR(CMAKE_GENERATOR MATCHES "Win64"))
			execute_process(COMMAND perl Configure VC-WIN64A no-shared --release --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		else()
			execute_process(COMMAND perl Configure VC-WIN32 no-asm no-shared --release --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		endif()

		execute_process(COMMAND nmake install WORKING_DIRECTORY ${directory})
	elseif(MACOS)
		execute_process(COMMAND ./Configure darwin64-x86_64-cc -fPIC no-shared --release --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make install WORKING_DIRECTORY ${directory})
	elseif(LINUX)
		execute_process(COMMAND ./config -fPIC no-shared no-tests --release --prefix=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME} WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make WORKING_DIRECTORY ${directory})
		execute_process(COMMAND make install WORKING_DIRECTORY ${directory})
	endif()
endif()
