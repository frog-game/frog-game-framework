local serviceCore = require "serviceCore"

local dataDB = {}

local command = {}

function command.get(key)
	return dataDB[key]
end

function command.set(key, value)
	local last = dataDB[key]
	dataDB[key] = value
	return last
end

function command.add(value1,value2)
    return value1+value2
end

function command.stop()
    serviceCore.async(serviceCore.exit)
end

serviceCore.start(function()
    serviceCore.eventDispatch(serviceCore.eventCall, function(_,cmd,...)
        serviceCore.log("example_call recv call cmd:" .. cmd );
        local f = command[cmd]
		if f then
            serviceCore.reply(f(...))
		else
			error(string.format("example_call unknown cmd: %s", tostring(cmd)))
		end
    end)
end)