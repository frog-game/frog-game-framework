local serviceCore = require "serviceCore"

local command = {}
local debugAddress = nil
local channelID = nil

function command.start(address,id)
    debugAddress = address
    channelID = id
    serviceCore.command(debugAddress,"_debug")
	return true;
end

function command.stop()
    channelID = nil
    serviceCore.command(debugAddress,"leave")
    serviceCore.async(serviceCore.exit)
    return true;
end

serviceCore.start(function()
	serviceCore.eventDispatch(serviceCore.eventCall, function(source,cmd,...)
        local func = assert(command[cmd])
        serviceCore.reply(func(...))
	end)

    serviceCore.eventDispatch(serviceCore.eventText, function(source,cmdline)
        serviceCore.command(debugAddress,cmdline)
    end)

    serviceCore.eventDispatch(serviceCore.eventCommand, function(source,cmd,s)
        if channelID and cmd == "_debugging" then
            serviceCore.remoteWrite(channelID, s)
        end
    end)
end)