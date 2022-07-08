local serviceCore = require "serviceCore"

local logConsole = true

local command = {}

function command.consoleOn()
    logConsole = true
end

function command.consoleOff()
    logConsole = false
end

serviceCore.start(function()
    serviceCore.eventDispatch(serviceCore.eventText, function(source, msg)
        local s = string.format("%08x: %s", source, msg)
        serviceCore.localPrint(s, logConsole)
    end)

    serviceCore.eventDispatch(serviceCore.eventCommand, function(_, cmd)
        local f = command[cmd]
        if f then
            f()
        end
    end)
end)
