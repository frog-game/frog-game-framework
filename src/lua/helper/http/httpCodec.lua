local lhttp = require "lruntime.http"

local httpCodec_t = {}

local http_meta = { __index = httpCodec_t }

function http_meta:__gc()
    lhttp.clear(self._obj)
end

function httpCodec_t.new(bResponse)
    local c = lhttp.new(bResponse)
    local self = {
        _obj = c
    }
    return setmetatable(self, http_meta)
end

function httpCodec_t.write(self,msg,length)
    return lhttp.write(self._obj,msg,length)
end

function httpCodec_t.read(self)
    return lhttp.read(self._obj)
end

function httpCodec_t.reset(self)
    return lhttp.reset(self._obj)
end

return httpCodec_t