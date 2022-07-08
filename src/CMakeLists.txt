add_subdirectory(
	core
)

add_subdirectory(
	service
)

add_subdirectory(
	runtime
)

# if(ENABLE_UNITTEST)
# add_subdirectory(
# unittest
# )
# endif()
if(ENABLE_BENCHMARK)
	add_subdirectory(
		benchmark
	)
endif()

add_subdirectory(
	serverBin
)

add_subdirectory(
	lua
)