
set(INSTALL_DATA_LUA_LIBRARY_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/library" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_SERVICE_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/service" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_HELPER_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/helper" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_HELPER_DB_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/helper/db" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_HELPER_HTTP_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/helper/http" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_EXAMPLE_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/example" CACHE PATH "Installation directory for lua")
set(INSTALL_DATA_LUA_SERVICE_EXAMPLE_DIR "${CMAKE_INSTALL_PREFIX}/bin/data/lua/service/example" CACHE PATH "Installation directory for lua")

set(LUA_LIBRARY_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/library/serviceCore.lua
	${CMAKE_CURRENT_SOURCE_DIR}/library/inject.lua
	${CMAKE_CURRENT_SOURCE_DIR}/library/injectcode.lua
	${CMAKE_CURRENT_SOURCE_DIR}/library/debugger.lua
)

set(LUA_SERVICE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/logService.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/monitorService.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/localServices.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/consoleService.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/debuggerService.lua
)

set(LUA_EXAMPLE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/example/start-up.lua
	${CMAKE_CURRENT_SOURCE_DIR}/example/config.lua
)

set(LUA_HELPER_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/helper/queueLock.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/logExt.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/channelPool.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/channelBlock.lua
)

set(LUA_HELPER_DB_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/helper/db/mysqlCodec.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/db/mysql.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/db/redisCodec.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/db/redis.lua
)

set(LUA_HELPER_HTTP_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/helper/http/url.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/http/httpCodec.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/http/request.lua
	${CMAKE_CURRENT_SOURCE_DIR}/helper/http/response.lua
)

set(LUA_SERVICE_EXAMPLE_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_call.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_callBuf.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_channel.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_command.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_console.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_crypt.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_dnsResolve.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_exit.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_httpRequest.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_httpResponse.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_mysql.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_queueLock.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_redis.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_sleep.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example_timer.lua
	${CMAKE_CURRENT_SOURCE_DIR}/service/example/example.lua
)

install(FILES ${LUA_LIBRARY_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_LIBRARY_DIR}")
install(FILES ${LUA_SERVICE_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_SERVICE_DIR}")
install(FILES ${LUA_HELPER_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_HELPER_DIR}")
install(FILES ${LUA_HELPER_DB_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_HELPER_DB_DIR}")
install(FILES ${LUA_HELPER_HTTP_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_HELPER_HTTP_DIR}")
install(FILES ${LUA_EXAMPLE_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_EXAMPLE_DIR}")
install(FILES ${LUA_SERVICE_EXAMPLE_SOURCE_FILES} DESTINATION "${INSTALL_DATA_LUA_SERVICE_EXAMPLE_DIR}")

