local lhttpTransfer = require "lruntime.httpTransfer"

local httpTransferCodec_t = {}

local http_meta = { __index = httpTransferCodec_t }

function http_meta:__gc()
    lhttpTransfer.clear(self._obj)
end

function httpTransferCodec_t.new(bResponse)
    local c = lhttpTransfer.new(bResponse)
    local self = {
        _obj = c
    }
    return setmetatable(self, http_meta)
end

function httpTransferCodec_t.write(self,msg,length)
    return lhttpTransfer.write(self._obj,msg,length)
end

function httpTransferCodec_t.readHeader(self)
    return lhttpTransfer.readHeader(self._obj)
end

function httpTransferCodec_t.readBody(self)
    return lhttpTransfer.readBody(self._obj)
end

function httpTransferCodec_t.reset(self)
    return lhttpTransfer.reset(self._obj)
end

return httpTransferCodec_t