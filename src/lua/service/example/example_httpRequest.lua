local serviceCore = require "serviceCore"
local httpRequest = require "http.request"
local logExt = require "logExt"

serviceCore.start(function()
	local succ,response= pcall(httpRequest.get,"http://127.0.0.1", "/testApi", nil, nil, 10000)
	if succ then
		logExt("example_httpRequest ",response)
	else
		serviceCore.log("example_httpRequest get error:"..response)
	end

	succ,response= pcall(httpRequest.get,"http://127.0.0.1", "/stop", nil, nil, 10000)
	if succ then
		logExt("example_httpRequest ",response)
	else
		serviceCore.log("example_httpRequest get error:"..response)
	end
end)