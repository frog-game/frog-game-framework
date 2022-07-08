local serviceCore = require "serviceCore"
local log = {}

log.level = "trace"

local eLevel = {
    elog_trace = 0,
    elog_debug = 1,
    eLog_info = 2,
    eLog_warning = 3,
    eLog_error = 4,
    eLog_fatal = 5,
}

local modes = {
    { name = "trace", level = eLevel.elog_trace, },
    { name = "debug", level = eLevel.elog_debug, },
    { name = "info", level = eLevel.eLog_info, },
    { name = "warn", level = eLevel.eLog_warning, },
    { name = "error", level = eLevel.eLog_error, },
    { name = "fatal", level = eLevel.eLog_fatal, },
}

local levels = {}
for i, v in ipairs(modes) do
    levels[v.name] = i
end


local round = function(x, increment)
    increment = increment or 1
    x = x / increment
    return (x > 0 and math.floor(x + .5) or math.ceil(x - .5)) * increment
end


local _tostring = tostring

local tostring = function(...)
    local t = {}
    for i = 1, select('#', ...) do
        local x = select(i, ...)
        if type(x) == "number" then
            x = round(x, .01)
        end
        t[#t + 1] = _tostring(x)
    end
    return table.concat(t, " ")
end


for i, x in ipairs(modes) do
    log[x.name] = function(...)
        -- Return early if we're below the log level
        if i < levels[log.level] then
            return
        end
        local msg = tostring(...)
        local info = debug.getinfo(2, "Sl")
        local lineinfo = info.short_src .. ":" .. info.currentline

        serviceCore.log(string.format("%d$%s: %s",
            x.level,
            lineinfo,
            msg))
    end
end


return log
