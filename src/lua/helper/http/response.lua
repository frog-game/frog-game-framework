local serviceCore = require "serviceCore"
local httpCodec = require "http.httpCodec"

local pairs = pairs

local urlCallbacks = {}

local _M = {}

local httpRequest_pool = setmetatable( {}, { __gc = function(p)
	for _,v in pairs(p) do
		if v.address then
			serviceCore.remoteClose(v.address)
		end
    end
end
})

local httpStatusMap_t = 
{
	[100] = "Continue",
    [101] = "Switching Protocols",
    [102] = "Processing",
	[200] = "OK",
	[201] = "Created",
	[202] = "Accepted",
	[203] = "Non-Authoritative Information",
	[204] = "No Content",
	[205] = "Reset Content",
	[206] = "Partial Content",
	[207] = "Multi-Status",
    [208] = "Already Reported",
    [226] = "IM Used",
	[300] = "Multiple Choices",
	[301] = "Moved Permanently",
	[302] = "Found",
	[303] = "See Other",
	[304] = "Not Modified",
	[305] = "Use Proxy",
    [307] = "Temporary Redirect",
    [308] = "Permanent Redirect",
	[400] = "Bad Request",
	[401] = "Unauthorized",
	[402] = "Payment Required",
	[403] = "Forbidden",
	[404] = "Not Found",
	[405] = "Method Not Allowed",
	[406] = "Not Acceptable",
	[407] = "Proxy Authentication Required",
	[408] = "Request Time-out",
	[409] = "Conflict",
	[410] = "Gone",
	[411] = "Length Required",
	[412] = "Precondition Failed",
	[413] = "Request Entity Too Large",
	[414] = "Request-URI Too Large",
	[415] = "Unsupported Media Type",
	[416] = "Requested range not satisfiable",
    [417] = "Expectation Failed",
    [421] = "Misdirected Request",
    [422] = "Unprocessable Entity",
    [423] = "Locked",
    [424] = "Failed Dependency",
    [426] = "Upgrade Required",
    [428] = "Precondition Required",
    [429] = "Too Many Requests",
    [431] = "Request Header Fields Too Large",
    [451] = "Unavailable For Legal Reasons",
	[500] = "Internal Server Error",
	[501] = "Not Implemented",
	[502] = "Bad Gateway",
	[503] = "Service Unavailable",
	[504] = "Gateway Time-out",
	[505] = "HTTP Version not supported",
	[506] = "Variant Also Negotiates",
	[507] = "Insufficient Storage",
	[508] = "Loop Detected",
	[510] = "Not Extended",
	[511] = "Network Authentication Required",
}


local function compose_message(statusCode, headers, body)
    local httpData = string.format("HTTP/1.1 %d %s \r\n", statusCode, httpStatusMap_t[statusCode] )

    if headers then
        for k,v in pairs(headers) do
            httpData =  httpData .. string.format("%s: %s\r\n", k, v )
        end
    end

    if body ~= nil then
		httpData = httpData .. string.format("content-length: %d\r\n\r\n", #body)
	else
		httpData = httpData .. "\r\n"
    end
    return httpData
end

local function response(httpRequest)
    return function (statusCode, headers, body)
		if not httpRequest.address then
            return
		end
		local httpData = compose_message(statusCode, headers, body)
		serviceCore.remoteWrite(httpRequest.address,httpData)
		if body then
			serviceCore.remoteWrite(httpRequest.address,body)
		end
    end
end

local httpCodec_pool = {}

local function codecAlloc()
	local codec
	local count = #httpCodec_pool
	if count > 0 then
		codec = httpCodec_pool[count]
		httpCodec_pool[count] = nil
	else
		codec = httpCodec.new(false)
	end
	return codec
end

local function codecRecovery(codec)
	httpCodec.reset(codec)
	httpCodec_pool[#httpCodec_pool+1] = codec
end

local bKeepAlive = true
local bTcpNoDelay = false

serviceCore.eventDispatch(serviceCore.eventAccept, function(source)
	if serviceCore.remoteBind(source,bKeepAlive,bTcpNoDelay) then
		local request = {}
		request.codec = codecAlloc()
		request.address = source
		request.response = response(request)
		httpRequest_pool[source] = request
	end
end)

serviceCore.eventDispatch(serviceCore.eventBinary, function(source,msg,length)
	local request = httpRequest_pool[source]
	if request ~= nil and request.address then
		local r = httpCodec.write(request.codec, msg,length)
		for i = 1,r do
			if not request.address then
				return
			end
			local data = httpCodec.read(request.codec)
			local func = urlCallbacks[data._path]
			if func then
				func(data,request.response)
			else
				request.response(400,nil,nil)
			end
		end
    end
end)

serviceCore.eventDispatch(serviceCore.eventDisconnect, function(source,...)
	local request = httpRequest_pool[source]
	if request ~= nil and request.address == source then
		serviceCore.remoteClose(request.address)
		request.address = nil
		codecRecovery(request.codec)
		request.codec = nil
		request.response = nil
		httpRequest_pool[source] = nil
	end
end)

function _M.register(uri,callbackFunc)
	urlCallbacks[uri] = callbackFunc
end

local listen

function _M.start(opts)
	if opts.keepAlive then
		bKeepAlive = opts.keepAlive
	end
	if opts.tcpNoDelay then
		bTcpNoDelay = opts.tcpNoDelay
	end

	listen = serviceCore.listenPort(opts.address);
	if listen == nil then
		return false;
	end
	return true;
end

function _M.stop()
	urlCallbacks = nil

	if listen then
		listen:close()
		listen = nil
	end

	for _, value in pairs(httpRequest_pool) do
		if value.address then
			serviceCore.remoteClose(value.address,5000)
			value.address = nil
			codecRecovery(value.codec)
			value.codec = nil
			value.response = nil
		end
	end
	httpRequest_pool = nil
	httpCodec_pool = nil
end

return _M