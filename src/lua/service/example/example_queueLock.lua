local serviceCore = require "serviceCore"
local queueLock = require "queueLock"

local dataDB = {}
local command = {}
local lock

function command.get(key)
	return lock( function()
        return dataDB[key]
    end)
end

function command.set(key, value)
    return lock( function()
        local last = dataDB[key]
        dataDB[key] = value
        serviceCore.sleep(1000)
        return last
    end)
end

function command.stop()
    serviceCore.async(serviceCore.exit)
end


serviceCore.start(function()
    lock = queueLock()
    serviceCore.eventDispatch(serviceCore.eventCommand, function(_,cmd,...)
        serviceCore.log("example_queueLock: recv cmd:" .. cmd );
        local f = command[cmd]
		if f then
            serviceCore.replyCommand(f(...))
		else
			error(string.format("unknown command %s", tostring(cmd)))
		end
    end)
end)