local serviceCore = require "serviceCore"
local redisCodec = require "db.redisCodec"
local channelPool = require "channelPool"

local disconnected <const> = 1
local connecting <const> = 2
local connected <const> = 3

local _M = {}
local command = {}
local redis_meta = {
	__index = command,
	__gc = function(self)
		if self.state ~= disconnected then
			serviceCore.remoteClose(self.address)
		end
	end
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

local function make_cache(f)
	return setmetatable({}, {
		__mode = "kv",
		__index = f,
	})
end

local header_cache = make_cache(function(t,k)
		local s = "\r\n$" .. k .. "\r\n"
		t[k] = s
		return s
	end)

local command_cache = make_cache(function(t,cmd)
		local s = "\r\n$"..#cmd.."\r\n"..cmd:upper()
		t[cmd] = s
		return s
	end)

local count_cache = make_cache(function(t,k)
		local s = "*" .. k
		t[k] = s
		return s
	end)

local function compose_message(cmd, msg)
	local lines = {}

	if type(msg) == "table" then
		lines[1] = count_cache[#msg+1]
		lines[2] = command_cache[cmd]
		local idx = 3
		for _,v in ipairs(msg) do
			v= tostring(v)
			lines[idx] = header_cache[#v]
			lines[idx+1] = v
			idx = idx + 2
		end
		lines[idx] = "\r\n"
	else
		msg = tostring(msg)
		lines[1] = "*2"
		lines[2] = command_cache[cmd]
		lines[3] = header_cache[#msg]
		lines[4] = msg
		lines[5] = "\r\n"
	end
	return table.concat(lines)
end

local function compose_table(lines, cmd)
	local cmds = {}
	for v in cmd:gmatch("%S+") do
        table.insert(cmds, v)
	end
	local index = #lines
	lines[index+1] = count_cache[#cmds]
	lines[index+2] = command_cache[string.upper(cmds[1])]
	local idx = 3
	for i = 2 , #cmds do
		local v = tostring(cmds[i])
		lines[index+idx] = header_cache[#v]
		lines[index+idx+1] = v
		idx = idx + 2
	end
	lines[index+idx] = "\r\n"
end

function command:cmdPipeline(t)
	assert(t and #t > 0, "cmd list is null")

	if self.state ~= connected then
		return false,"disconnect"
	end

	local cmds = {}
	for _, cmd in ipairs(t) do
		compose_table(cmds, cmd)
	end
	local s = table.concat(cmds)
	serviceCore.sendBuf(self.address,s)
	local co = coroutine.running()
	table.insert(self.wait_response,co)
	serviceCore.wait(co)
	if self.state ~= connected then
		return false,"disconnect"
	end
	
	local result ={}
	for i=1, #t do
		table.insert(result, self.codec:read())
	end
	return result
end

function command:message()
	if self.state ~= connected then
		return false,"disconnect"
	end
	local result = self.codec:read()
	if result then
		return result
	end

	local co = coroutine.running()
	table.insert(self.wait_response,co)
	serviceCore.wait(co)
	if self.state ~= connected then
		return false,"disconnect"
	end

	result = self.codec:read()
	if result then
		return result
	end
end

function command:disconnect()
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

function _M.connect(opts)
	local self = setmetatable({}, redis_meta)
	
    self.state = connecting

	local address = serviceCore.connect(opts.address,10000);
    if not address then
        self.state = disconnected
        return false
	end

    serviceCore.remoteBind(address,false,true)
	
	self.address = address
	self.wait_response = {}
	self.codec = redisCodec.new()

    channelPool.attach(address,dispatch_disconnect(self),dispatch_write(self))
	
	if opts.auth then
		local s = compose_message("AUTH",opts.auth)
		serviceCore.sendBuf(self.address,s)
		local co = coroutine.running()
		table.insert(self.wait_response,co)
		serviceCore.wait(co)
		if self.state ~= connecting then
			return false,"disconnect"
		end

		local result = self.codec:read()
		if result ~= "OK" then
			return false, result
		end
	end

	if opts.db then
		local s = compose_message("SELECT",opts.db)
		serviceCore.sendBuf(self.address,s)
		local co = coroutine.running()
		table.insert(self.wait_response,co)
		serviceCore.wait(co)
		if self.state ~= connecting then
			return false,"disconnect"
		end
		local result = self.codec:read()
		if result ~= "OK" then
			return false, result
		end
	end

    self.state = connected
    return self
end

setmetatable(command, { __index = function(t,k)
	local cmd = string.upper(k)
	local f = function (self, ...)
		if self.state ~= connected then
			return false,"disconnect"
		end
		local s = compose_message(cmd, {...})
		serviceCore.sendBuf(self.address,s)
		local co = coroutine.running()
		table.insert(self.wait_response,co)
		serviceCore.wait(co)
		if self.state ~= connected then
			return false,"disconnect"
		end
		return self.codec:read()
	end
	t[k] = f
	return f
end})

return _M
