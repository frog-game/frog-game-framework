-- modify from skynet https://github.com/cloudwu/skynet/ inject.lua

local function getupvaluetable(u, func, unique)
	local i = 1
	while true do
		local name, value = debug.getupvalue(func, i)
		if name == nil then
			return
		end
		local t = type(value)
		if t == "table" then
			u[name] = value
		elseif t == "function" then
			if not unique[value] then
				unique[value] = true
				getupvaluetable(u, value, unique)
			end
		end
		i=i+1
	end
end

local eventName_t <const> =
{
	[1] = "call",
	[2] = "ping",
	[3] = "close",
	[4] = "send",
	[5] = "text",
	[6] = "binary",
	[7] = "disconnect",
	[8] = "command",
	[9] = "accept"
}

return function(serviceCore, source, filename, args, ...)
	if filename then
		filename = "@" .. filename
	else
		filename = "=(load)"
	end
	local output = {}

	local function print(...)
		local value = { ... }
		for k,v in ipairs(value) do
			value[k] = tostring(v)
		end
		table.insert(output, table.concat(value, "\t"))
	end
	local u = {}
	local unique = {}
	local funcs = { ... }
	for k, func in ipairs(funcs) do
		getupvaluetable(u, func, unique)
	end
	local p = {}
	local eventDispatch = u.eventDispatch_t
	if eventDispatch then
		for k,v in pairs(eventDispatch) do
			local event, dispatch = k, v
			if dispatch then
				local pp = {}
				p[eventName_t[event]] = pp
				getupvaluetable(pp, dispatch, unique)
			end
		end
	end
	
	local func
	local err
	local env = setmetatable( { print = print , _U = u, _P = p}, { __index = _ENV })
	func, err = load(source, filename, "bt", env)
	if not func then
		return false, { err }
	end
	local ok
	ok,err = serviceCore.pcall(func, table.unpack(args, 1, args.n))
	if not ok then
		table.insert(output, err)
		return false, output
	end

	return true, output
end
