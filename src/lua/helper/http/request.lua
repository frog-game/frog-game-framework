local serviceCore = require "serviceCore"
local httpCodec = require "http.httpCodec"

local assert = assert
local pairs = pairs

local nConnectTimeout = 10000

local _M = {}

local httpRequest_pool = setmetatable( {}, { __gc = function(p)
	for _,v in pairs(p) do
		if v.address then
			serviceCore.remoteClose(v.address)
		end
    end
end
})

local function wakeup_f(s)
	local co = s.co
	if co then
		s.co = nil
		serviceCore.wakeup(co)
	end
end

local function suspend_f(s,timeout)
	assert(not s.co)
	s.co = coroutine.running()
	if timeout then
		return serviceCore.sleep(timeout,s.co)
	else
		serviceCore.wait(s.co)
		return "BREAK"
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
		codec = httpCodec.new(true)
	end
	return codec
end

local function codecRecovery(codec)
	httpCodec.reset(codec)
	httpCodec_pool[#httpCodec_pool+1] = codec
end

serviceCore.eventDispatch(serviceCore.eventBinary, function(source,msg,length)
	local request = httpRequest_pool[source]
	if request ~= nil then
		local r = httpCodec.write(request.codec, msg,length)
		if r == 1 then
			wakeup_f(request)
		end
    end
end)

serviceCore.eventDispatch(serviceCore.eventDisconnect, function(source,...)
	local request = httpRequest_pool[source]
	if request~= nil then
		serviceCore.timeout(5000,function()
			if request.address then
				request.connect = false;
				wakeup_f(request);
			end
		end)
	end
end)

local function getDomainName(host)
	local protocol = host:match("^[Hh][Tt][Tt][Pp][Ss]?://")
	if protocol then
		host = string.gsub(host, "^"..protocol, "")
		protocol = string.lower(protocol)
		if protocol == "https://" then
			return host, "https"
		elseif protocol == "http://" then
			return host,"http"
		else
			return host
		end
	else
		return host,"http"
	end
end


local function compose_message(method, path, query, headers, body)
    local httpData
    if query == nil then
        httpData = string.format("%s %s HTTP/1.1\r\n", method, path )
    else
        httpData = string.format("%s %s?%s HTTP/1.1\r\n", method, path, query )
    end

    if headers then
        for k,v in pairs(headers) do
            httpData = string.format("%s%s: %s\r\n",httpData, k, v )
        end
	end
	
	if body then
		httpData = httpData .. string.format("content-length: %d\r\n\r\n", #body)
	else
		httpData = httpData .. "\r\n"
	end

    return httpData
end

function _M.setConnectTimeout(connectTimeout)
	nConnectTimeout = connectTimeout
end

function _M.call(method, host, path, query, headers, body,timeout)
	local protocol
	host, protocol = getDomainName(host)
	
	local hostname, port = host:match"([^:]+):?(%d*)$"
	if port == "" then
		port = protocol=="http" and 80 or protocol=="https" and 443
	else
		port = tonumber(port)
	end
	if not hostname:match(".*%d+$") then
		local t = serviceCore.dnsResolve(hostname,true)
		if not t then
			error(string.format("%s dnsResolve error host:%s, port:%s", protocol, hostname, port))
		end
		hostname = t[1]
	end

	local address = serviceCore.connect(string.format("%s:%d",hostname,port),nConnectTimeout);
	if not address then
		error(string.format("%s connect error host:%s, port:%s, timeout:%s", protocol, hostname, port, nConnectTimeout))
	end

	serviceCore.remoteBind(address,true,false)

	headers = headers or {}

	if not headers.host then
		headers.host = host
	end

	if headers["Connection"] == nil then
		headers["Connection"] = "close"
	end

	local httpData = compose_message(method,path,query,headers,body)

	serviceCore.remoteWrite(address,httpData)
	if body then
		serviceCore.remoteWrite(address,body)
	end

	local request = {}
	request.co = false
	request.codec = codecAlloc()
	request.address = address
	request.connect = true
	httpRequest_pool[address] = request

	if not suspend_f(request,timeout) then
		request.address = nil
		serviceCore.remoteClose(address,5000)
		httpRequest_pool[address] = nil
		codecRecovery(request.codec)
		request.codec = nil
		error("http request wait timeout")
	end

	if not request.connect then
		request.address = nil
		serviceCore.remoteClose(address,5000)
		httpRequest_pool[address] = nil
		codecRecovery(request.codec)
		request.codec = nil
		error("http disconnect")
	end

	local response = httpCodec.read(request.codec)
	request.address = nil
	serviceCore.remoteClose(address,5000)
	httpRequest_pool[address] = nil
	codecRecovery(request.codec)
	request.codec = nil
	return response
end

function _M.get(host, path, query, headers,timeout)
	return _M.call("GET", host, path, query, headers, nil,timeout)
end

function _M.postForm(host, path, headers, form, timeout)
	local body = {}

	for key, value in pairs(form) do
		table.insert(body, string.format("%s=%s",key,value))
	end

	headers = headers or {}
	headers["content-type"] = "application/x-www-form-urlencoded"
	return _M.call("POST", host, path, nil, headers, table.concat(body , "&"),timeout)
end

return _M
