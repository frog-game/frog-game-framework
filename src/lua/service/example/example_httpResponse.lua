local serviceCore = require "serviceCore"
local httpResponse = require "http.response"
local logExt = require "logExt"

local function test(request,response)
    logExt("example_httpResponse ",request)
    response(200,nil,"OK")
end

local function stop(request,response)
    logExt("example_httpResponse ",request)
    response(200,nil,"OK")
    serviceCore.timeout(1000,serviceCore.exit)
end


serviceCore.start(function()
    httpResponse.register("/testApi",test)
    httpResponse.register("/stop",stop)
    httpResponse.start({address = "127.0.0.1:80"})
end)