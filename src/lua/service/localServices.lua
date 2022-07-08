local serviceCore = require "serviceCore"

local services = {}
local command = {}

function command.new(filename,param)
	local serviceId = serviceCore.createService(filename,param)
	if serviceId ~= 0 then
		services[serviceId] = { filename,param }
	end
	return serviceCore.replyCommand(serviceId);
end

function command.remove(serviceId)
	services[serviceId] = nil
end

function command.mem()
	local list = {}
	for k,v in pairs(services) do
		local ok, kb = pcall(serviceCore.callCommand,k,"_mem")
		if not ok then
			list[serviceCore.addressToString(k)] = string.format("error (%s:%s)",v[1],v[2])
		else
			list[serviceCore.addressToString(k)] = string.format("%.2f Kb (%s:%s)",kb,v[1],v[2])
		end
	end
	serviceCore.replyCommand(list)
end

function command.gc()
	for k,_ in pairs(services) do
		serviceCore.command(k,"_gc")
	end
end

function command.list()
	local list = {}
	for k,v in pairs(services) do
		list[serviceCore.addressToString(k)] = string.format("(%s:%s)",v[1],v[2])
	end
	serviceCore.replyCommand(list)
end

function command.status()
	local list = {}
	for k,v in pairs(services) do
		local ok, status = pcall(serviceCore.callCommand,k,"_status")
		if not ok then
			status = string.format("error (%s:%s)",v[1],v[2])
		end
		list[serviceCore.addressToString(k)] = status
	end
	serviceCore.replyCommand(list)
end


serviceCore.start(function()
	serviceCore.eventDispatch(serviceCore.eventCommand,function(_,cmd,...)
		local f = command[cmd]
		if f then
			f(...)
		end
	end)
end)