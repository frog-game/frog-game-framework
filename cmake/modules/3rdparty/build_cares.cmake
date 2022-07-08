set(BUILD_3RDPARTY_NAME "cares")
set(BUILD_3RDPARTY_FILENAME "c-ares-cares-1_15_0.tar.gz")

message(STATUS "BUILD NAME:(cares)")

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

file(MAKE_DIRECTORY ${directory}/build)

if(DEBUG_3RDPARTY_OUTPUT)
  set(BUILD_COMMAND_OPTS --target install --config Debug)
else()
  set(BUILD_COMMAND_OPTS --target install --config Release)
endif()

set(CARES_CONFIG -DCARES_SHARED=OFF -DCARES_STATIC=ON -DCARES_STATIC_PIC=ON -DCARES_BUILD_TOOLS=OFF)

if(MSVC AND MSVC_USE_STATIC_RUNTIME_LIBRARY)
  set(CARES_CONFIG ${CARES_CONFIG} -DCARES_MSVC_STATIC_RUNTIME=ON)
endif()

execute_process(COMMAND ${CMAKE_COMMAND}
  -DCMAKE_INSTALL_PREFIX=${FROG_3RDPARTY_BINARY_DIR}/install/${BUILD_3RDPARTY_NAME}
  -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
  -DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}
  -DCMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
  -G ${CMAKE_GENERATOR}
  ${CARES_CONFIG}
  ${3RDPARTY_BUILD_COMMAND}
  ${directory}
  WORKING_DIRECTORY ${directory}/build)

execute_process(COMMAND ${CMAKE_COMMAND}
  --build ${directory}/build ${BUILD_COMMAND_OPTS} WORKING_DIRECTORY ${directory}/build)
