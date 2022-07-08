local serviceCore = require "serviceCore"

local tDump = {}

local maxLevel = 32

local function dump_f(s)
    tDump[#tDump+1] = s
end

local function sub_table(t,l)
    l = l + 1
    local s = string.rep("\t",l)
    if l <= maxLevel then
        for k,v in pairs(t) do
            if type(v) == "table" then
                dump_f(string.format("%s[%s]={",s,k))
                sub_table(v,l)
                dump_f(string.format("%s}",s))
            elseif type(v) == "string" then
                dump_f(string.format('%s[%s]="%s"',s,k,tostring(v)))
            else
                dump_f(string.format("%s[%s]=%s",s,k,tostring(v)))
            end
        end
    else
        dump_f(string.format("%s__level_limit",s))
    end
end

local function dump_table(t)
    dump_f(string.format("[%s]={",tostring(t)))
    sub_table(t,0)
    dump_f("}")
end

local function log_extend(...)
    local t = { ... }
    for _,v in ipairs(t) do
        if type(v) == "table" then
            dump_table(v)
        else
            dump_f(tostring(v))
        end
    end
    serviceCore.log(table.concat(tDump,"\n"))
    tDump = {}
end

local function setMaxLevel(l)
    assert(math.type(l) == "integer")
    maxLevel = l
end

return log_extend,setMaxLevel