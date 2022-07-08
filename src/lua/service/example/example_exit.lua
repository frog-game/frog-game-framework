local launchParam = ...
local serviceCore = require "serviceCore"

local command = {}

function command.stop()
    serviceCore.async(serviceCore.exit)
    serviceCore.log("example_exit exit")
end

serviceCore.start(function()
	if launchParam == "exit" then
        serviceCore.log("example_exit exit")
        serviceCore.exit()
    else
        serviceCore.eventDispatch(serviceCore.eventCommand, function(_,cmd,...)
            serviceCore.log("example_exit recv cmd:" .. cmd );
            local f = command[cmd]
            if f then
                serviceCore.reply(f(...))
            else
                error(string.format("example_exit unknown cmd: %s", tostring(cmd)))
            end
        end)
    end
end)