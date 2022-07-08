local lredis = require "lruntime.redis"

local redisCodec_t = {}

local redis_meta = { __index = redisCodec_t }

function redisCodec_t.new()
    local c = lredis.new()
    local self = {
        _obj = c
    }
    return setmetatable(self, redis_meta)
end

function redisCodec_t:write(msg,length)
    return lredis.write(self._obj,msg,length)
end

function redisCodec_t:read()
    return lredis.read(self._obj)
end

function redisCodec_t:reset()
    return lredis.reset(self._obj)
end

return redisCodec_t