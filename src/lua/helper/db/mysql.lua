local serviceCore = require "serviceCore"
local lcrypt = require "lruntime.crypt"
local mysqlCodec = require "db.mysqlCodec"
local channelPool = require "channelPool"

local disconnected <const> = 1
local connecting <const> = 2
local connected <const> = 3

local CLIENT_PROTOCOL_41 <const>		= 0x00000200
local CLIENT_SECURE_CONNECTION <const>	= 0x00008000
local CLIENT_CONNECT_WITH_DB <const>	= 0x00000008
local CLIENT_PLUGIN_AUTH <const>		= 0x00080000
local CLIENT_MULTI_STATEMENTS <const>	= 0x00010000
local CLIENT_MULTI_RESULTS <const>		= 0x00020000
local CLIENT_PS_MULTI_RESULTS <const>	= 0x00040000
local CLIENT_LOCAL_FILES <const>		= 0x00000080
local COM_QUERY <const> = 0x00000003


local _M = {}

local mysql_meta = {
    __index = _M
}

local function dispatch_write(self)
    return function (msg,len)
        if self.state == disconnected then
            return
        end
        local r = self.codec:write(msg,len)
        if r > 0 then
            for i = 1, r, 1 do
                local co = table.remove(self.wait_response, 1)
                serviceCore.wakeup(co)
            end
        end
    end
end

local function dispatch_disconnect(self)
    return function()
		if self.state ~= disconnected then
            self.state = disconnected
            for _, co in ipairs(self.wait_response) do
                serviceCore.wakeup(co)
            end
            serviceCore.remoteClose(self.address)
            self.address = nil
            self.codec = nil
        end
        setmetatable(self, nil)
    end
end

local function compute_token(password, scramble)
    if password == "" then
        return ""
    end

    local stage1 = lcrypt.sha1(password)
    local stage2 = lcrypt.sha1(stage1)
    local stage3 = lcrypt.sha1(scramble .. stage2)

    local i = 0
	return string.gsub(stage3,".",
		function(x)
			i = i + 1
			return string.char(string.byte(x) ~ string.byte(stage1, i))
        end)
end

function _M.connect(opts)
    local self = setmetatable({}, mysql_meta)

    self.state = connecting

	local address = serviceCore.connect(opts.address,10000);
    if not address then
        self.state = disconnected
        return false,"connect error"
    end

    serviceCore.remoteBind(address,false,true,mysqlCodec.codecHandle())
    
    channelPool.attach(address,dispatch_disconnect(self),dispatch_write(self))

    self.address = address
    self.wait_response = {}
    local co = coroutine.running()
    table.insert(self.wait_response,co)
    self.codec = mysqlCodec.new()
    serviceCore.wait(co)
    if self.state ~= connecting then
        return false,"disconnect"
    end

    local s = self.codec:read()

    local clientFlags = CLIENT_PROTOCOL_41|CLIENT_SECURE_CONNECTION|CLIENT_CONNECT_WITH_DB|CLIENT_PLUGIN_AUTH|CLIENT_MULTI_STATEMENTS|CLIENT_MULTI_RESULTS|CLIENT_PS_MULTI_RESULTS|CLIENT_LOCAL_FILES

    local user = opts.user or ""
    local password = opts.password or ""
    local database = opts.db or ""

    self.codec:reset(false)
    -- if opts.use_ssl then
    -- end
    local token = compute_token(password, s.scramble)

    local charset = (opts.charset == nil) and 33 or opts.charset

    local req = string.pack("<I4I4c1c23zs1zz",
        clientFlags,
        0,
        string.char(charset),
        string.rep("\0", 23),
        user,
        token,
        database,
        "mysql_native_password"
    )

    serviceCore.sendBuf(self.address,req)
    co = coroutine.running()
    table.insert(self.wait_response,co)
    serviceCore.wait(co)
    if self.state ~= connecting then
        return false,"disconnect"
    end
    local result = self.codec:read()
    if result.type == "error" then
        self.state = disconnected
        return false, result.errorMsg
    end
    self.state = connected
    return self
end

function _M.query(self, query, array)
    if self.state ~= connected then
        return nil
    end
    local req = string.char(COM_QUERY) .. query
    serviceCore.sendBuf(self.address,req)
    local co = coroutine.running()
    table.insert(self.wait_response,co)
    serviceCore.wait(co)
    if self.state ~= connected then
        return nil
    end
    return self.codec:read(array)
end


function _M.disconnect(self)
	if self.state ~= disconnected then
        self.state = disconnected
        channelPool.detach(self.address)
        serviceCore.remoteClose(self.address,5000)
        self.address = nil
		for _, co in ipairs(self.wait_response) do
			serviceCore.wakeup(co)
		end

		self.codec = nil
	end
	setmetatable(self, nil)
end

return _M