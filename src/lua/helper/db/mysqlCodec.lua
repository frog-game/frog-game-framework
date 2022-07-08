local lmysql = require "lruntime.mysql"

local mysqlCodec_t = {}

local mysql_meta = { __index = mysqlCodec_t }

function mysqlCodec_t.new()
    local c = lmysql.new()
    local self = {
        _obj = c
    }
    return setmetatable(self, mysql_meta)
end

function mysqlCodec_t.write(self,msg,length)
    return lmysql.write(self._obj,msg,length)
end

function mysqlCodec_t.read(self,array)
    return lmysql.read(self._obj,array)
end

function mysqlCodec_t.reset(self,isHandshake)
    return lmysql.reset(self._obj,isHandshake)
end

mysqlCodec_t.codecHandle = lmysql.codecHandle

return mysqlCodec_t