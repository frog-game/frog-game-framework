local serviceCore = require "serviceCore"

local coroutine = coroutine
local xpcall = xpcall
local traceback = debug.traceback
local table = table

local function queueLock()
	local running_co
	local ref = 0
	local queue = {}

	local function xpcall_ret_f(ok, ...)
		ref = ref - 1
		if ref == 0 then
			running_co = table.remove(queue,1)
			if running_co then
				serviceCore.wakeup(running_co)
			end
		end
		assert(ok, (...))
		return ...
	end

	return function(f, ...)
		local co = coroutine.running()
		if running_co and running_co ~= co then
			table.insert(queue, co)
			serviceCore.wait()
			assert(ref == 0)
		end
		running_co = co

		ref = ref + 1
		return xpcall_ret_f(xpcall(f, traceback, ...))
	end
end

return queueLock
