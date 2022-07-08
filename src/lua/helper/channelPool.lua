local serviceCore = require "serviceCore"

local _M = {}

local channels = setmetatable({},{
    __gc = function(pool)
        for _,v in pairs(pool) do
            if v.disconnect then
                v.disconnect()
            end
        end
    end
})

serviceCore.eventDispatch(serviceCore.eventBinary, function(source,msg,length)
    local t = channels[source]
    if t and t.write then
        t.write(msg,length)
    end
end)

serviceCore.eventDispatch(serviceCore.eventDisconnect, function(source)
    local t = channels[source]
    if t and t.disconnect then
        t.disconnect()
        channels[source] = nil
    end
end)

function _M.attach(address,fnDisconnect,fnWrite)
    local t = {}
    t.disconnect = fnDisconnect
    t.write = fnWrite
    channels[address] = t
end

function _M.detach(address)
    channels[address] = nil
end

function _M.close()
    for _,v in pairs(channels) do
        if v.disconnect then
            v.disconnect()
        end
    end
end

return _M

