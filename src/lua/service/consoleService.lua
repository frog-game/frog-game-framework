local serviceCore = require "serviceCore"
local channelBlock = require "channelBlock"
local lenv = require "lruntime.env"
local lchannelExt = require "lruntime.channelExt"

local command = {}
local listen = nil

local cmdlineCommand = {}
local codeCommand = {}

local function split_cmdline(cmdline)
	local split = {}
	for i in string.gmatch(cmdline, "%S+") do
		table.insert(split,i)
	end
	return split
end

local function format_table(t)
	local index = {}
	for k in pairs(t) do
		table.insert(index, k)
	end
	table.sort(index, function(a, b) return tostring(a) < tostring(b) end)
	local result = {}
	for _,v in ipairs(index) do
		table.insert(result, string.format("%s:%s",v,tostring(t[v])))
	end
	return table.concat(result,"\t")
end

local function dump_line(print, key, value)
	if type(value) == "table" then
		print(key, format_table(value))
	else
		print(key,tostring(value))
	end
end

local function dump_list(print, list)
	local index = {}
	for k in pairs(list) do
		table.insert(index, k)
	end
	table.sort(index, function(a, b) return tostring(a) < tostring(b) end)
	for _,v in ipairs(index) do
		dump_line(print, v, list[v])
	end
end

function cmdlineCommand.help()
	return {
		help = "show help cmd",
		services = "show all service list",
		status = "show all service status",
		luamem = "show all service lua state memory",
		luagc = " all service run collectgarbage \"collect\"",
		exit = "exit service. exit address",
		launch = "lanuch a new lua service. launch filename [opt param]",
		inject = "service run (*.lua) file. inject address filename [opt ...]",
		info = "get service infomation. info address ...",
		logon =  "log on. logon address",
		logoff =  "log off. logoff address",
		profileon =  "profile on. profileon address",
		profileoff =  "profile off. profileoff address",
		ping = "test service. ping address",
		call = "run call service. call address cmdline",
		callCommand = "run callCommand service. callCommand address cmdline",
		cacheon = "lua file cache on",
		cacheoff = "lua file cache off",
		cacheabandon = "abandon a lua file cache. cacheabandon filename",
		channels = " all channel status",
		debughelp = "show debug help cmd",
		debug = "start a service debugger. debug address"
	}
end

function cmdlineCommand.services()
	return serviceCore.callCommand("_localS","list")
end

function cmdlineCommand.status()
	return serviceCore.callCommand("_localS", "status")
end

function cmdlineCommand.luamem()
	return serviceCore.callCommand("_localS", "mem")
end

function cmdlineCommand.luagc()
	serviceCore.command("_localS", "gc")
end

function cmdlineCommand.cacheon()
	lenv.luacacheOn()
end

function cmdlineCommand.cacheoff()
	lenv.luacacheOff()
end

function cmdlineCommand.channels()
	local list = {}
	local channelIDs = lchannelExt.gets()
	if channelIDs then
		for _,v in ipairs(channelIDs) do
			local t = {}
			t.writePending = lchannelExt.writePending(v)
			t.writePendingBytes = lchannelExt.writePendingBytes(v)
			t.receiveBufLength = lchannelExt.receiveBufLength(v)
			t.remoteAddr = lchannelExt.remoteAddr(v,true)
			t.localAddr = lchannelExt.localAddr(v,true)
			list[serviceCore.addressToString(v)] = t
		end
	end
	return list
end

function cmdlineCommand.cacheabandon(filename)
	lenv.luacacheAbandon(filename)
end

function cmdlineCommand.exit(address)
	serviceCore.command(address,"_exit")
end

function cmdlineCommand.launch(...)
	local ok, addr = pcall(serviceCore.launch, ...)
	if ok then
		if addr then
			return { [serviceCore.addressToString(addr)] = ... }
		end
	end
	return "launch fail"
end

function cmdlineCommand.inject(address, filename, ...)
	local f = io.open(filename, "rb")
	if not f then
		return "Can't open " .. filename
	end
	local source = f:read "*a"
	f:close()
	local ok, output = serviceCore.callCommand(address, "_run", source, filename, ...)
	if ok == false then
		error(output)
	end
	return output
end

function cmdlineCommand.info(address, ...)
	return serviceCore.callCommand(address,"_info", ...)
end

function cmdlineCommand.logon(address)
	serviceCore.command(address,"_logon")
end

function cmdlineCommand.logoff(address)
	serviceCore.command(address,"_logoff")
end

function cmdlineCommand.profileon(address)
	serviceCore.command(address,"_profileon")
end

function cmdlineCommand.profileoff(address)
	serviceCore.command(address,"_profileoff")
end

function cmdlineCommand.ping(address)
	local timer = serviceCore.getClockMonotonic()
	local ok = pcall(serviceCore.ping, address)
	if ok then
		timer = serviceCore.getClockMonotonic() - timer
		return tostring(timer)
	end
	return "ping address error"
end

function cmdlineCommand.debughelp()
	return {
		watch = "breakpoint event. watch \"event\" [opt function(...)]  event:call ping close send text binary disconnect command accept",
		s = "run next line, stepping into function calls",
		n = "run next line, stepping over function calls",
		c = "continue",
		_co = "coroutine object. view stack: debug.traceback(_co)",
		p = "view variables. p(variables)",
		leave = "exit debugger"
	}
end

function codeCommand.debug(cmd)
	local address = tonumber(cmd[2])
	if pcall(serviceCore.ping, address) then
		local service = serviceCore.launch("debuggerService")
		serviceCore.call(service,"start",address,cmd.channel._address)
		cmd.channel._debugger = service
	end
end

function codeCommand.call(cmd)
	local address = cmd[2]
	local cmdline = assert(cmd[1]:match("%S+%s+%S+%s(.+)") , "need arguments")
	local args_func = assert(load("return " .. cmdline, "console", "t", {}), "Invalid arguments")
	local args = table.pack(pcall(args_func))
	if not args[1] then
		error(args[2])
	end
	local rets = table.pack(serviceCore.call(address, table.unpack(args, 2, args.n)))
	return rets
end

function codeCommand.callCommand(cmd)
	local address = cmd[2]
	local cmdline = assert(cmd[1]:match("%S+%s+%S+%s(.+)") , "need arguments")
	local args_func = assert(load("return " .. cmdline, "console", "t", {}), "Invalid arguments")
	local args = table.pack(pcall(args_func))
	if not args[1] then
		error(args[2])
	end
	local rets = table.pack(serviceCore.callCommand(address, table.unpack(args, 2, args.n)))
	return rets
end


local function cmdlineDispatch_f(channel,cmdline,print)
	local split = split_cmdline(cmdline)
	local cmd = split[1]
	local f = cmdlineCommand[cmd]
	local ok, list
	if f then
		ok, list = pcall(f, table.unpack(split,2))
	else
		f = codeCommand[cmd]
		if f then
			split.channel = channel
			split[1] = cmdline
			ok, list = pcall(f,split)
		else
			print("<invalid command")
			return
		end
	end

	if ok then
		if list then
			if type(list) == "string" then
				print(list)
			else
				dump_list(print, list)
			end
		end
		print("<cmd success")
	else
		print("<cmd error")
		print(list)
	end
end

local function debuggingDispatch_f(channel,cmdline)
	cmdline = cmdline and cmdline:gsub("(.*)\r$", "%1")
	if not cmdline or cmdline == "leave" then
		serviceCore.call(channel._debugger,"stop")
		channel._debugger = nil
	else
		serviceCore.sendText(channel._debugger,cmdline)
	end
end

local function consoleRun(channel,print)
	print("<console connected")
	local ok, err = pcall(function()
		local offset = 0
		local cmdline = nil
		while true do
			cmdline, offset = channel:readLineEOL(offset)
			if not cmdline then
				break
			end
			if channel._debugger then
				debuggingDispatch_f(channel,cmdline)
			else
				cmdlineDispatch_f(channel,cmdline,print)
			end
		end
	end)

	if not ok then
		serviceCore.log("console:" .. channel._address .. "error:" .. err)
	end

	if channel._debugger then
		serviceCore.call(channel._debugger,"stop")
		channel._debugger = nil
	end
	channel:close()
end

function command.start(szAddress)
	listen = serviceCore.listenPort(szAddress);
	if listen == nil then
		serviceCore.log("listen error address:" .. szAddress)
		return false;
	end
	serviceCore.log("console start listen:" .. szAddress)
	return true;
end

function command.stop()
	if listen then
		listen:close()
		listen = nil
	end
	channelBlock.closeAll()
	return true;
end

serviceCore.start(function()
	serviceCore.bindName("console")

	serviceCore.eventDispatch(serviceCore.eventCall, function(_,cmd,...)
		local func = assert(command[cmd])
		serviceCore.reply(func(...))
	end)

	serviceCore.eventDispatch(serviceCore.eventAccept, function(source,...)
		if source == 0 then
			return
		end
		if serviceCore.remoteBind(source,true,false) then
			local function print(...)
				local t = { ... }
				for k,v in ipairs(t) do
					t[k] = tostring(v)
				end
				serviceCore.remoteWrite(source,table.concat(t,"\t"))
				serviceCore.remoteWrite(source,"\r\n")
			end

			local channel = channelBlock.open(source)
			serviceCore.async(consoleRun,channel,print)
		end
	end)
end)