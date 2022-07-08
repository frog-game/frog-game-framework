local serviceCore = require "serviceCore"
local ltpack = require "lruntime.tpack"

local bUdp
local command = {}
local remoteCmd = {}
local dataDB = {}
local listen = nil
local channelID = nil

function command.start(sAddress,UDP)
	bUdp = UDP
	listen = serviceCore.listenPort(sAddress,bUdp)
	if listen == nil then
		if bUdp then
			serviceCore.log("example_channel udp error address:" .. sAddress)
		else
			serviceCore.log("example_channel tcp error address:" .. sAddress)
		end
		return false;
	end
	if bUdp then
		serviceCore.log("example_channel udp start listen:" .. sAddress)
	else
		serviceCore.log("example_channel tcp start listen:" .. sAddress)
	end
	return true;
end

function command.stop()
	if channelID then
		serviceCore.remoteClose(channelID)
		channelID = nil
	end
	if listen then
		listen:close()
		listen = nil
	end
	return true;
end


function remoteCmd.get(key)
	return dataDB[key]
end

function remoteCmd.set(key, value)
	local last = dataDB[key]
	dataDB[key] = value
	return last
end

function remoteCmd.add(value1,value2)
    return value1+value2
end

function remoteCmd.test(sTest)
	if bUdp then
		serviceCore.log("example_channel udp remote test:" .. sTest)
	else
		serviceCore.log("example_channel tcp remote test:" .. sTest)
	end
	return "hello world"
end

serviceCore.start(function()
	serviceCore.eventDispatch(serviceCore.eventCall, function(source,cmd,...)
		if bUdp then
			serviceCore.log("example_channel udp call cmd:" .. cmd)
		else
			serviceCore.log("example_channel tcp call cmd:" .. cmd)
		end
		local func = assert(remoteCmd[cmd])
		serviceCore.reply(func(...))
	end)

	serviceCore.eventDispatch(serviceCore.eventSend, function(source,cmd,...)
		if bUdp then
			serviceCore.log("example_channel udp send cmd:" .. cmd)
		else
			serviceCore.log("example_channel tcp send cmd:" .. cmd)
		end
		local func = assert(remoteCmd[cmd])
		func(...)
	end)

	serviceCore.eventDispatch(serviceCore.eventCommand, function(source,cmd,...)
		serviceCore.log("example_channel command cmd:" .. cmd)
		local func = assert(command[cmd])
		serviceCore.reply(func(...))
	end)

	serviceCore.eventDispatch(serviceCore.eventAccept, function(source,msg,sz)
		if source == 0 then
			return
		end

		if bUdp then
			serviceCore.log("example_channel udp eventAccept:" .. source .. " recv data:" .. serviceCore.cbufferToString(msg, sz))
		else
			serviceCore.log("example_channel tcp eventAccept:" .. source)
		end

		if channelID then
			serviceCore.remoteClose(source)
		end
		if bUdp then
			if serviceCore.remoteBind(source,false,false,ltpack.codecHandle()) then
				serviceCore.log("example_channel udp eventAccept success")
			end
		else
			if serviceCore.remoteBind(source,false,true,ltpack.codecHandle()) then
				serviceCore.log("example_channel tcp eventAccept success")
			end
		end
		channelID = source
	end)

	serviceCore.eventDispatch(serviceCore.eventDisconnect, function(source,...)
		if source then
			if bUdp then
				serviceCore.log("example_channel udp Disconnect:".. source)
			else
				serviceCore.log("example_channel tcp Disconnect:".. source)
			end
			if channelID == source then
				serviceCore.remoteClose(channelID)
				channelID = nil
			end
		end
	end)
end)