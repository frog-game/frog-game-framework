local serviceCore = require "serviceCore"
local lbuffer = require "lruntime.buffer"
local channelPool = require "channelPool"

local _M = {}

local channel_meta = {
	__index = _M,
}

local function wakeup_f(self)
	local co = self._co
	if co then
		self._co = nil
		serviceCore.wakeup(co)
		return true
	end
	return false
end

local function suspend_f(self)
	assert(not self._co)
	self._co = coroutine.running()
	serviceCore.wait(self._co)
end

local function dispatch_write(self)
	return function(msg,length)
		if self._running then
			if lbuffer.write(self._buf,msg,length) >= self._bufLimit then
				_M.close(self._address)
			else
				wakeup_f(self)
			end
		end
    end
end

local function dispatch_disconnect(self)
	return function()
		if self._running then
			self._running = false
			serviceCore.remoteClose(self._address)
			if wakeup_f(self) then
				self._closing = coroutine.running()
				serviceCore.wait(self._closing)
			end
			lbuffer.clear(self._buf)
		end
    end
end

function _M.closeAll()
	channelPool.close()
end

function _M.open(address)
    local self = setmetatable({}, channel_meta)
	self._address = address
	self._running = true
	self._buf = lbuffer.new()
	self._bufLimit = 8192
	channelPool.attach(address,dispatch_disconnect(self),dispatch_write(self))
	return self
end

function _M.setBufferLimit(self, limit)
	self._bufLimit = limit
end

function _M.close(self)
	if self._running then
		self._running = false
		channelPool.detach(self._address)
		serviceCore.remoteClose(self._address,5000)
		if wakeup_f(self) then
			self._closing = coroutine.running()
			serviceCore.wait(self._closing)
		end
		lbuffer.clear(self._buf)
	else
		local co = self._closing
		if co then
			self._closing = nil
			serviceCore.wakeup(co)
		end
	end
end

function _M.readAll(self)
	while self._running do
		local s = lbuffer.readAll(self._buf)
		if s ~= nil then
			return s
		end
		suspend_f(self)
	end
	return false
end

function _M.read(self, n)
	if n == nil then
		return _M.readAll(self),0
	end

	while self._running do
		local s, readable = lbuffer.read(self._buf,n)
		if s ~= nil then
			return s,readable
		end
		suspend_f(self)
	end
	return false, 0
end


function _M.readLineEOL(self,offset)
	while self._running do
		local s, n = lbuffer.readLineEOL(self._buf,offset)
		if s ~= nil then
			return s,n
		end
		suspend_f(self)
	end
	return false, 0
end

function _M.readLineCRLF(self,offset)
	while self._running do
		local s, n = lbuffer.readLineCRLF(self._buf,offset)
		if s ~= nil then
			return s,n
		end
		suspend_f(self)
	end
	return false, 0
end

return _M
