local lservice = require "lruntime.service"
local lmsgpack = require "lruntime.msgpack"

local table_t = table
local table_remove_f = table_t.remove
local table_insert_f = table_t.insert
local table_unpack_f = table_t.unpack

local select_f = select
local tostring_f = tostring
local pairs_f = pairs
local ipairs_f = ipairs
local pcall_f = pcall
local xpcall_f = xpcall

local assert_f = assert
local error_f = error

local coroutine_t = coroutine
local coroutine_create_f = coroutine_t.create
local coroutine_yield_f = coroutine_t.yield
local coroutine_resume_f = coroutine_t.resume

local string_t = string
local string_format_f = string_t.format

local debug_t = debug

local debug_getinfo_f = debug_t.getinfo
local debug_traceback_f = debug_t.traceback

local msgCodec_t = lmsgpack

local eventDispatch_t = {}
local runEveryDispatch_t = {}

local eventBinary <const> 		= 1
local eventDisconnect <const>	= 2
local eventSendOK <const> 		= 3
local eventAccept <const> 		= 4
local eventConnect <const> 		= 5
local eventDns <const> 			= 6
local eventYield <const> 		= 7
local eventRunAfter <const> 	= 8
local eventRunEvery <const> 	= 9
local eventMsg <const> 			= 10
local eventCommand <const> 		= 11
local eventMask <const> 		= 0x0F

local eventMsgReply <const> 	= 0x70
local eventMsgCall <const> 		= 0x60
local eventMsgPing <const> 		= 0x50
local eventMsgPong <const> 		= 0x40
local eventMsgClose <const> 	= 0x30
local eventMsgSend <const> 		= 0x20
local eventMsgText <const> 		= 0x10
local eventMsgMask <const> 		= 0xF0

local serviceCore =
{
	eventCall 				= 1,
	eventPing 				= 2,
	eventClose 				= 3,
	eventSend 				= 4,
	eventText 				= 5,
	eventBinary 			= 6,
	eventDisconnect 		= 7,
	eventCommand 			= 8,
	eventAccept 			= 9
}

function serviceCore.addressToString(address)
	if type(address) == "number" then
		return string_format_f("%08x",address)
	else
		return tostring_f(address)
	end
end

function serviceCore.isRemoteId(serviceId)
	return (serviceId & 0x80000000) ~=0
end

local running_co = nil
local tokenToCoroutine_t = {}
local coroutineToToken_t = {}
local coroutineToAddress_t = {}

local wakeupQueue_t = {}
local cancelWatchingQueue_t = {}

local sleepCoroutineToToken_t = {}
local watchingTokenToAddress_t = {}
local responseToAddress_t = {}

local function co_resume_f(co, ...)
	running_co = co
	return coroutine_resume_f(co, ...)
end

local coroutineReuse_t = setmetatable({}, { __mode = "kv" })

local function co_create_f(func)
	local co = table_remove_f(coroutineReuse_t)
	if co == nil then
		co = coroutine_create_f(function(...)
			func(...)
			while true do
				local token = coroutineToToken_t[co]
				if token and token ~= 0 then
					local info = debug_getinfo_f(func,"S")
					lservice.log(string_format_f("Maybe frogot response token %d from %s : %s:%d",token,serviceCore.addressToString(coroutineToAddress_t[co]),info.source, info.linedefined))
				end
				local address = coroutineToAddress_t[co]
				if address then
					coroutineToToken_t[co] = nil
					coroutineToAddress_t[co] = nil
				end
				func = nil
				coroutineReuse_t[#coroutineReuse_t+1] = co
				func = coroutine_yield_f("SUSPEND")
				func(coroutine_yield_f())
			end
		end)
	else
		local _co = running_co
		co_resume_f(co, func)
		running_co = _co
	end
	return co
end

local asyncQueue_t = {}

function serviceCore.async(func,...)
	local n = select_f("#", ...)
	local co
	if n == 0 then
		co = co_create_f(func)
	else
		local args = { ... }
		co = co_create_f(function() func(table_unpack_f(args,1,n)) end)
	end
	table_insert_f(asyncQueue_t, co)
	return co
end

local suspend_f

local function dispatch_wakeup_f()
	local co = table_remove_f(wakeupQueue_t,1)
	if co then
		local token = sleepCoroutineToToken_t[co]
		if token then
			local co_ = tokenToCoroutine_t[token]
			tokenToCoroutine_t[token] = "BREAK"
			return suspend_f(co_, co_resume_f(co_, false, "BREAK"))
		end
	end
end

local function dispatch_watching_cancel_f()
	local token = table_remove_f(cancelWatchingQueue_t,1)
	if token then
		local co = tokenToCoroutine_t[token]
		tokenToCoroutine_t[token] = nil
		return suspend_f(co, co_resume_f(co, false))
	end
end

function suspend_f(co, result, command)
	if not result then
		local token = coroutineToToken_t[co]
		if token then
			coroutineToAddress_t[co] = nil;
			coroutineToToken_t[co] = nil
		end
		serviceCore.async(function() end)
		error_f(debug_traceback_f(co,tostring_f(command)))
	end
	if command == "SUSPEND" then
		dispatch_wakeup_f()
		dispatch_watching_cancel_f()
	elseif command == "QUIT" then
		return
	elseif command == nil then
		return
	else
		error_f("Unknown command : " .. command .. "\n".. debug_traceback_f(co))
	end
end

local function unknown_reply_f(source, token, msg, length)
    lservice.log(string_format_f("unknown reply source:%08x token:%d (%s)",source,token,lservice.cbufferToString(msg,length)))
    error_f(string_format_f("unknown reply source:%08x token:%d (%s)", source,token,lservice.cbufferToString(msg,length) ))
end

local function unknown_call_f(source, token, msg, length)
    lservice.log(string_format_f("unknown call source:%08x token:%d (%s)",source,token,lservice.cbufferToString(msg,length)))
    error_f(string_format_f("unknown call source:%08x token:%d (%s)", source,token,lservice.cbufferToString(msg,length) ))
end

local function unknown_dispatch_f(event, source, token)
    lservice.log(string_format_f("unknown dispatch event:%d source:%08x token:%d",event,source,token))
    error_f(string_format_f("unknown dispatch event:%d source:%08x token:%d", event,source,token))
end

function serviceCore.setUnknownReply(unknownFunc)
	local prev = unknown_reply_f
	unknown_reply_f = unknownFunc
	return prev
end

function serviceCore.setUnknownCall(unknownFunc)
	local prev = unknown_call_f
	unknown_call_f = unknownFunc
	return prev
end

function serviceCore.setUnknownDispatch(unknownFunc)
	local prev = unknown_dispatch_f
	unknown_dispatch_f = unknownFunc
	return prev
end

local function close_dispatch_f(source,token)
	coroutineToToken_t[running_co] = nil
	if token == 0 then
		for response,address in pairs_f(responseToAddress_t) do
			if address == source then
				responseToAddress_t[response] = nil
			end
		end

		for token_, address in pairs_f(watchingTokenToAddress_t) do
			if source == address then
				table_insert_f(cancelWatchingQueue_t, token_)
			end
		end
	else
		if watchingTokenToAddress_t[token] then
			table_insert_f(cancelWatchingQueue_t, token)
		end
	end
end

local function event_msg_dispatch_f(eventMsg, source, token, msg, length)
	if eventMsg == eventMsgReply then
		local co = tokenToCoroutine_t[token]
		if co == "BREAK" then
			tokenToCoroutine_t[token] = nil
		elseif co == nil then
			unknown_reply_f(source,token,msg,length)
		else
			tokenToCoroutine_t[token] = nil
			suspend_f(co,co_resume_f(co,true,msg,length))
		end
	elseif eventMsg == eventMsgCall then
		local func = eventDispatch_t[serviceCore.eventCall]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			coroutineToToken_t[co] = token
			suspend_f(co,co_resume_f(co,source,msgCodec_t.decode(msg,length)))
		else
			if token ~= 0 then
				lservice.sendClose(source,token)
			else
				unknown_call_f(source,token,msg,length)
			end
		end 
	elseif eventMsg == eventMsgPing then
		local func = eventDispatch_t[serviceCore.eventPing]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			coroutineToToken_t[co] = token
			suspend_f(co,co_resume_f(co,source,token))
		else
			return lservice.pong(source,token)
		end
	elseif eventMsg == eventMsgPong then
		local co = tokenToCoroutine_t[token]
		if co == "BREAK" then
			tokenToCoroutine_t[token] = nil
		elseif co == nil then
			unknown_dispatch_f(eventMsg,source,token)
		else
			tokenToCoroutine_t[token] = nil
			suspend_f(co,co_resume_f(co,true))
		end
	elseif eventMsg == eventMsgClose then
		local func = eventDispatch_t[serviceCore.eventClose]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			suspend_f(co, co_resume_f(co,source,token,close_dispatch_f))
		else
			local co = co_create_f(close_dispatch_f)
			suspend_f(co, co_resume_f(co,source,token))
		end
	elseif eventMsg == eventMsgSend then
		local func = eventDispatch_t[serviceCore.eventSend]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			suspend_f(co, co_resume_f(co,source,msgCodec_t.decode(msg,length)))
		else
			unknown_dispatch_f(eventMsg,source,token)
		end
	elseif eventMsg == eventMsgText then
		local func = eventDispatch_t[serviceCore.eventText]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			suspend_f(co, co_resume_f(co,source,lservice.cbufferToString(msg,length)))
		else
			unknown_dispatch_f(eventMsg,source,token)
		end
	end
end

local defaultCommand = {}

local function command_f(func,source,cmd,...)
	local cmdFunc = defaultCommand[cmd]
	if cmdFunc then
		cmdFunc(...)
	else
		func(source,cmd,...)
	end
end

local function eventDispatch_f(event, source, token, msg, length)
	local type = event & eventMask
	if type == eventMsg then
		event_msg_dispatch_f(event & eventMsgMask,source,token,msg,length);
	elseif type == eventRunEvery then
		local func = runEveryDispatch_t[token]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			suspend_f(co, co_resume_f(co,false))
		else
			unknown_dispatch_f(type, source, token)
		end
	elseif type == eventYield or type == eventRunAfter or type == eventSendOK then
		local co = tokenToCoroutine_t[token]
		if co == "BREAK" then
			tokenToCoroutine_t[token] = nil
		elseif co == nil then
			unknown_dispatch_f(event,source,token)
		else
			tokenToCoroutine_t[token] = nil
			suspend_f(co, co_resume_f(co,true))
		end
	elseif type == eventConnect then
		local co = tokenToCoroutine_t[token]
		if co == "BREAK" then
			tokenToCoroutine_t[token] = nil
		elseif co == nil then
			unknown_dispatch_f(event,source,token)
		else
			tokenToCoroutine_t[token] = nil
			suspend_f(co, co_resume_f(co,source))
		end
	elseif type == eventDns then
		local co = tokenToCoroutine_t[token]
		if co == "BREAK" then
			tokenToCoroutine_t[token] = nil
		elseif co == nil then
			unknown_dispatch_f(event,source,token)
		else
			tokenToCoroutine_t[token] = nil
			suspend_f(co, co_resume_f(co,msg,length))
		end
	elseif type == eventAccept or type == eventDisconnect or type == eventBinary then
		local func = eventDispatch_t[type+5]
		if func then
			local co = co_create_f(func)
			coroutineToAddress_t[co] = source
			suspend_f(co, co_resume_f(co,source,msg,length))
		else
			unknown_dispatch_f(type, source,0)
		end
	elseif type == eventCommand then
		local func = eventDispatch_t[serviceCore.eventCommand]
		local co = co_create_f(command_f)
		coroutineToAddress_t[co] = source
		coroutineToToken_t[co] = token
		suspend_f(co, co_resume_f(co,func,source,lmsgpack.decode(msg,length)))
	else
		unknown_dispatch_f(type, source, token)
	end
end

local function yield_call_f(address, token)
	watchingTokenToAddress_t[token] = address
	tokenToCoroutine_t[token] = running_co
	local succ, msg, sz = coroutine_yield_f("SUSPEND")
	watchingTokenToAddress_t[token] = nil
	if not succ then
		error_f("call failed")
	end
	return msg, sz
end

local function yield_watching_f(address, token)
	watchingTokenToAddress_t[token] = address
	tokenToCoroutine_t[token] = running_co
	return coroutine_yield_f("SUSPEND")
end

function serviceCore.yield(func)
	local token = lservice.yield()
	local co = co_create_f(func)
	assert_f(tokenToCoroutine_t[token] == nil)
	tokenToCoroutine_t[token] = co
	return co
end

function serviceCore.timeout(intervalMs, func)
	local token = lservice.timeout(intervalMs)
	local co = co_create_f(func)
	assert_f(tokenToCoroutine_t[token] == nil)
	tokenToCoroutine_t[token] = co
	return token
end

function serviceCore.runEvery(intervalMs, func)
	local token = #runEveryDispatch_t + 1
	local timer = lservice.runEvery(intervalMs,token)
	local refCount = 1
	local runFunc = function (stop)
		refCount = refCount + 1
		if stop or not func() then
			if timer:stop() then
				refCount = refCount -1
			end
		end
		
		refCount = refCount -1
		if refCount == 0 then
			runEveryDispatch_t[token] = nil
		end
	end
	runEveryDispatch_t[token] = runFunc
	return token
end

function serviceCore.stopRunEvery(token)
	local func = runEveryDispatch_t[token]
	if  func ~= nil then
		func(false);
	end
end

local function suspend_sleep_f(co, token)
	tokenToCoroutine_t[token] = running_co
	assert_f(sleepCoroutineToToken_t[co] == nil, "coroutine repeat")
	sleepCoroutineToToken_t[co] = token
	return coroutine_yield_f("SUSPEND")
end

function serviceCore.sleep(intervalMs, co)
	local timer <close>, token = lservice.runAfter(intervalMs)
	assert_f(token)
	co = co or coroutine_t.running()
	local succ, ret = suspend_sleep_f(co, token)
	sleepCoroutineToToken_t[co] = nil
	if succ then
		return
	end
	if ret == "BREAK" then
		return "BREAK"
	else
		error_f(ret)
	end
end

function serviceCore.wait(co)
	local token = lservice.genToken()
	co = co or coroutine_t.running()
	suspend_sleep_f(co,token)
	sleepCoroutineToToken_t[co] = nil
	tokenToCoroutine_t[token] = nil
end

function serviceCore.wakeup(co)
	if sleepCoroutineToToken_t[co] then
		table_insert_f(wakeupQueue_t, co)
		return true
	end
end

function serviceCore.callBuf(address, msg, sz)
	local token = lservice.send(address,eventMsgCall,nil,msg,sz)
	if token == nil then
		error_f("call to invalid " .. serviceCore.addressToString(address))
	end
	return yield_call_f(address, token)
end

function serviceCore.call(address, ...)
	return msgCodec_t.decode(serviceCore.callBuf(address, msgCodec_t.encode(...)))
end

function serviceCore.command(address, ...)
	return lservice.command(address, 0, lmsgpack.encode(...) ) ~= nil
end

function serviceCore.callCommand(address, ...)
	local token = lservice.command(address,nil,lmsgpack.encode(...))
	if token == nil then
		error_f("command call to invalid " .. serviceCore.addressToString(address))
	end
	return lmsgpack.decode(yield_call_f(address, token))
end

function serviceCore.redirect(sourceId, address, event, token, msg, sz)
	if not lservice.redirect(sourceId,address,event,token,msg,sz) then
		error_f("redirect to invalid " .. serviceCore.addressToString(sourceId))
	end
end

function serviceCore.redirectCallBuf(address,msg, sz)
	local token = assert_f(coroutineToToken_t[running_co], "no token")
	coroutineToToken_t[running_co] = nil
	if token == 0 then
        return false
	end
	local sourceId = coroutineToAddress_t[running_co]
	if sourceId == nil then
		error_f("no sourceId")
	end
	serviceCore.redirect(sourceId,address,serviceCore.eventCall,token,msg,sz)
end

function serviceCore.redirectCall(address,...)
	serviceCore.redirectCallBuf(address,msgCodec_t.encode(...))
end

function serviceCore.replyBuf(msg, sz)
	msg = msg or ""
	local token = assert_f(coroutineToToken_t[running_co], "no token")
	coroutineToToken_t[running_co] = nil
	if token == 0 then
        return false
	end
	local address = coroutineToAddress_t[running_co]
	if address == nil then
		error_f("no address")
	end
	return lservice.send(address, eventMsgReply,token, msg, sz) ~= nil
end

function serviceCore.reply(...)
	return serviceCore.replyBuf(msgCodec_t.encode(...))
end

function serviceCore.replyCommand(...)
	if coroutineToToken_t[running_co] == 0 then
		return false
	end
	return serviceCore.replyBuf(lmsgpack.encode(...))
end

function serviceCore.replyClose()
	local token = assert_f(coroutineToToken_t[running_co], "no token")
	coroutineToToken_t[running_co] = nil
	if token == 0 then
        return false
	end
	local address = coroutineToAddress_t[running_co]
	if address == nil then
		error_f("no address")
	end
	return lservice.sendClose(address,token)
end

function serviceCore.sendText(address, msg, sz)
	return lservice.send(address, eventMsgText,0,msg, sz) ~= nil
end

function serviceCore.sendBuf(address, msg, sz)
	return lservice.send(address, eventMsgSend,0,msg, sz) ~= nil
end

function serviceCore.send(address, ...)
	return serviceCore.sendBuf(address, msgCodec_t.encode(...))
end

function serviceCore.genResponse(encode)
	encode = encode or msgCodec_t.encode
	local token = assert_f(coroutineToToken_t[running_co], "no token")
	coroutineToToken_t[running_co] = nil
	local address = coroutineToAddress_t[running_co]
	if token == 0 then
		return function() end
	end

	local function response(ok, ...)
		if not encode then
			error_f "can't response more than once"
		end

		local ret
		if responseToAddress_t[response] then
			if ok then
				ret = lservice.send(address, eventMsgReply,token, encode(...))
			else
				ret = lservice.sendClose(address,token)
			end
			responseToAddress_t[response] = nil
			ret = ret ~= nil
		else
			ret = false
		end
		encode = nil
		return ret
	end
	responseToAddress_t[response] = address
	return response

end

function serviceCore.pong(address)
	if address == nil then
		local token = assert_f(coroutineToToken_t[running_co],"no token")
		coroutineToToken_t[running_co] = nil
		if token == 0 then
			return false
		end
		local address_ = coroutineToAddress_t[running_co]
		if address_ == nil then
			error_f("no address")
		end
		return lservice.pong(address_,token)
	else
		return lservice.pong(address,nil)
	end
end

function serviceCore.ping(address)
	local token = lservice.ping(address)
	if token == nil then
		error_f("ping to invalid" .. serviceCore.addressToString(address))
	end
	local succ = yield_watching_f(address, token)
	watchingTokenToAddress_t[token] = nil
	return succ
end


function serviceCore.connect(ipAddr,connectingTimeoutMs,udp)
	local connector <close>, token = lservice.connect(ipAddr,connectingTimeoutMs,udp)
	if not connector then
		return nil
	end
	tokenToCoroutine_t[token] = running_co
	local address = coroutine_yield_f("SUSPEND")
	if address == 0 then
		return nil
	end
	return address
end

function serviceCore.dnsResolve(s,lookHostsFile,bIPv6)
	local dns <close>, token = lservice.dnsResolve(s,lookHostsFile,bIPv6)
	if not dns then
		return nil
	end
	tokenToCoroutine_t[token] = running_co
	local msg, sz = coroutine_yield_f("SUSPEND")
	local dnsAddrs = dns:parser(msg,sz)
	return dnsAddrs
end

function serviceCore.getRunningAddress()
	return coroutineToAddress_t[running_co]
end

function serviceCore.getRunningToken()
	return coroutineToToken_t[running_co]
end

function serviceCore.clearRunningToken()
	coroutineToToken_t[running_co] = nil
end

function serviceCore.dispatch(...)
	local succ,err = pcall_f(eventDispatch_f,...)
	while true do
		local co = table_remove_f(asyncQueue_t,1)
		if co == nil then
			break
		end
		local succ_, err_ = pcall_f(suspend_f,co,co_resume_f(co))
		if not succ_ then
			if succ then
				succ = false
				err = tostring_f(err_)
			else
				err = tostring_f(err) .. "\n" .. tostring_f(err_)
			end
		end
	end
	assert_f(succ, tostring_f(err))
end

function serviceCore.setMsgCodec(codec)
	if codec then
		local ret = msgCodec_t
		msgCodec_t = codec
		return ret
	else
		return msgCodec_t
	end
end

function serviceCore.eventDispatch(type,func)
	if func then
		local ret = eventDispatch_t[type]
		eventDispatch_t[type] = func
		return ret
	else
		return eventDispatch_t[type]
	end
end

local internalInfo_f

function serviceCore.internalInfo(func)
	if func then
		local ret = internalInfo_f
		internalInfo_f = func
		return ret
	else
		return internalInfo_f
	end
end

serviceCore.sendClose = lservice.sendClose
serviceCore.self = lservice.self
serviceCore.log = lservice.log
serviceCore.localPrint = lservice.localPrint
serviceCore.bindName = lservice.bindName
serviceCore.createService = lservice.createService
serviceCore.findService = lservice.findService
serviceCore.bindServiceName = lservice.bindServiceName
serviceCore.unbindServiceName = lservice.unbindServiceName
serviceCore.cbufferToString = lservice.cbufferToString
serviceCore.remoteClose = lservice.remoteClose
serviceCore.remoteBind = lservice.remoteBind
serviceCore.listenPort = lservice.listenPort
serviceCore.hardwareConcurrency = lservice.hardwareConcurrency
serviceCore.getClockMonotonic = lservice.getClockMonotonic
serviceCore.getClockRealtime = lservice.getClockRealtime

function serviceCore.remoteWrite(address, msg, sz) -- remote
	if not lservice.remoteWrite(address, msg, sz) then
		error_f("remoteWrite to invalid address:" .. serviceCore.addressToString(address))
	end
end

function serviceCore.remoteWriteReq(address, msg, sz) -- remote
	local token = lservice.remoteWriteReq(address,msg,sz)
	if token == nil then
		error_f("remoteWriteReq to invalid address:" .. serviceCore.addressToString(address))
	end
	tokenToCoroutine_t[token] = running_co
	local succ = coroutine_yield_f("SUSPEND")
	watchingTokenToAddress_t[token] = nil
	return succ
end

function serviceCore.launch(filename, param)
	return serviceCore.callCommand("_localS", "new", filename, param)
end

function serviceCore.term(serviceId)
	close_dispatch_f(serviceId,0)
end

function serviceCore.exit()
	asyncQueue_t = {}
	serviceCore.command("_localS", "remove", serviceCore.self())
	for co, token in pairs_f(coroutineToToken_t) do
		local address = coroutineToAddress_t[co]
		if token ~=0 and address then
			lservice.sendClose(address,token)
		end
	end

	for response, _ in pairs_f(responseToAddress_t) do
		response(false)
	end

	for _, runFunc in pairs_f(runEveryDispatch_t) do
		runFunc(false)
	end

	local watchingAddressStatus = {}
	for _, address  in pairs_f(watchingTokenToAddress_t) do
		watchingAddressStatus[address] = true
	end

	for address,_ in pairs_f(watchingAddressStatus) do
		lservice.sendClose(address,0)
	end
	lservice.exit()
end


local initFunc_t = {}

function serviceCore.loadInitFun(func, name)
	assert_f(type(func) == "function")
	if initFunc_t == nil then
		func()
	else
		table_insert_f(initFunc_t, func)
		if name then
			assert_f(type(name) == "string")
			assert_f(initFunc_t[name] == nil)
			initFunc_t[name] = func
		end
	end
end

local function init_all_func_f()
	local func_t = initFunc_t
	initFunc_t = nil
	if func_t then
		for _,func in ipairs_f(func_t) do
			func()
		end
	end
end

local function ret_f(func, ...)
	func()
	return ...
end

local function init_template_f(func, ...)
	init_all_func_f()
	initFunc_t = {}
	return ret_f(init_all_func_f, func(...))
end

function serviceCore.pcall(func, ...)
	return xpcall_f(init_template_f, debug_traceback_f, func, ...)
end

function serviceCore.init(func)
	local succ, err = serviceCore.pcall(func)
	if not succ then
		serviceCore.log("init context failed: " .. tostring_f(err))
		serviceCore.exit()
	end
end

function serviceCore.start(func)
	lservice.setCallback(serviceCore.dispatch)
	serviceCore.yield(function()
		serviceCore.init(func)
	end)
end

--defaultCommand
function defaultCommand._mem()
	local kb = collectgarbage "count"
	serviceCore.replyCommand(kb)
end

function defaultCommand._gc()
	collectgarbage "collect"
end

function defaultCommand._status()
	local status = {}
	status.cost,status.count,status.queue = lservice.status()
	serviceCore.replyCommand(status)
end

function defaultCommand._info(...)
	if internalInfo_f then
		serviceCore.replyCommand(internalInfo_f(...))
	else
		serviceCore.replyCommand(nil)
	end
end

function defaultCommand._logon()
	lservice.setLog(true)
end

function defaultCommand._logoff()
	lservice.setLog(false)
end

function defaultCommand._profileon()
	lservice.setProfile(true)
end

function defaultCommand._profileoff()
	lservice.setProfile(false)
end

function defaultCommand._run(source, filename, ...)
	local inject = require "inject"
	local args = table.pack(...)
	local ok, output = inject(serviceCore, source, filename, args, serviceCore.dispatch, serviceCore.eventDispatch)
	collectgarbage "collect"
	serviceCore.replyCommand(ok, table.concat(output, "\r\n"))
end

function defaultCommand._exit()
	serviceCore.exit()
end

function defaultCommand._term(serviceId)
	serviceCore.term(serviceId)
end

function defaultCommand._debug()
	local debuggerInit = require "debugger"
	debuggerInit(suspend_f,co_resume_f,eventDispatch_f)
end

return serviceCore