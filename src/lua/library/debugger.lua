local serviceCore = require "serviceCore"
local injectcode = require "injectcode"
local ldebug = require "lruntime.debug"
local table = table
local debug = debug
local coroutine = coroutine
local ldebug_sethook_f = ldebug.sethook

local HOOK_FUNC <const> = "command_f"
local command_f
local suspend_f
local resume_f

local print = _G.print

local function replace_upvalue(func, uvname, value)
	local i = 1
	while true do
		local name, uv = debug.getupvalue(func, i)
		if name == nil then
			break
		end
		if name == uvname then
			if value then
				debug.setupvalue(func, i, value)
			end
			return uv
		end
		i = i + 1
	end
end

local function remove_hook(dispatch)
	assert(command_f, "not in debug mode")
	replace_upvalue(dispatch, HOOK_FUNC, command_f)
	command_f = nil
	print = _G.print
	serviceCore.log("leave debug mode:".. serviceCore.addressToString(serviceCore.self()))
end

local function gen_print(address)
	return function(...)
		local tmp = table.pack(...)
		for i=1,tmp.n do
			tmp[i] = tostring(tmp[i])
		end
		table.insert(tmp, "\r\n")
		serviceCore.command(address, "_debugging",table.concat(tmp, "\t"))
	end
end

local function run_exp(ok, ...)
	if ok then
		print(...)
	end
	return ok
end

local function run_cmd(cmd, env, co, level)
	serviceCore.log("run_cmd:".. cmd)
	if not run_exp(injectcode("return "..cmd, co, level, env)) then
		print(select(2, injectcode(cmd,co, level,env)))
	end
end

local ctx_serviceCore = debug.getinfo(serviceCore.start,"S").short_src	-- skip when enter this source file
local ctx_term = debug.getinfo(run_cmd, "S").short_src	-- term when get here
local ctx_active = {}

local linehook

local function skip_hook(mode)
	local co = coroutine.running()
	local ctx = ctx_active[co]
	if mode == "return" then
		ctx.level = ctx.level - 1
		if ctx.level == 0 then
			ctx.needupdate = true
			ldebug_sethook_f(linehook, "crl")
		end
	elseif mode == "call" then
		ctx.level = ctx.level + 1
	end
end

function linehook(mode, line)
	local co = coroutine.running()
	local ctx = ctx_active[co]
	if mode ~= "line" then
		ctx.needupdate = true
		if mode ~= "return" then
			if ctx.next_mode or debug.getinfo(2,"S").short_src == ctx_serviceCore then
				ctx.level = 1
				ldebug_sethook_f(skip_hook, "cr")
			end
		end
	else
		if ctx.needupdate then
			ctx.needupdate = false
			ctx.filename = debug.getinfo(2, "S").short_src
			if ctx.filename == ctx_term then
				ctx_active[co] = nil
				ldebug_sethook_f()
				print(string.format(":%08x>", serviceCore.self()))
				return
			end
		end
		print(string.format("%s(%d)>",ctx.filename, line))
		return true	-- yield
	end
end

local function add_watch_hook()
	local co = coroutine.running()
	local ctx = { level = 0 }
	ctx_active[co] = ctx
	local level = 1
	ldebug_sethook_f(function(mode)
		if mode == "return" then
			level = level - 1
		elseif mode == "call" then
			level = level + 1
			if level == 0 then
				ctx.needupdate = true
				ldebug_sethook_f(linehook, "crl")
			end
		else
			if level == 0 then
				ctx.needupdate = true
				ldebug_sethook_f(linehook, "crl")
			end
		end
	end, "cr")
end


local function remove_watch()
	for co in pairs(ctx_active) do
		ldebug_sethook_f(co)
	end
	ctx_active = {}
end


local event_t =
{
    call = serviceCore.eventCall,
    ping = serviceCore.eventPing,
    close = serviceCore.eventClose,
    send = serviceCore.eventSend,
    text = serviceCore.eventText,
    binary = serviceCore.eventBinary,
	disconnect = serviceCore.eventDisconnect,
	command = serviceCore.eventCommand,
    accept = serviceCore.eventAccept
}

local function watch_event(eventName, opt)
	local eventDispatch = assert(replace_upvalue(serviceCore.eventDispatch, "eventDispatch_t"), "can't find eventDispatch_t")
    local eventId = event_t[eventName]
    if eventId == nil then
		return "eventName error:" .. eventName
	end
    local func = eventDispatch[eventId]
	if func == nil then
		return "eventDispatch nil:" .. eventName
	end
	eventDispatch[eventId] = function(...)
		if not opt or opt(...) then
            eventDispatch[eventId] = func	
			add_watch_hook()
			func(...)
			remove_watch()
		else
			func(...)
		end
	end
end

local command = {}

function command.s(co)
	local ctx = ctx_active[co]
	if ctx.level == 0 then
		ctx.next_mode = false
		suspend_f(co, resume_f(co))
	else
		print("wait")
	end
end

function command.n(co)
	local ctx = ctx_active[co]
	if ctx.level == 0 then
		ctx.next_mode = true
		suspend_f(co, resume_f(co))
	else
		print("wait")
	end
end

function command.c(co)
	ldebug_sethook_f(co)
	ctx_active[co] = nil
	print(string.format(":%08x>", serviceCore.self()))
	suspend_f(co, resume_f(co))
end

local function debuggerInit(fnSuspend,fnResume,fnEventDispatch)
	suspend_f = fnSuspend
    resume_f = fnResume

	local eventDispatch = fnEventDispatch
	
	local debugger = serviceCore.getRunningAddress()
	serviceCore.log("debug mode:".. serviceCore.addressToString(serviceCore.self())) 

	print = gen_print(debugger)

	local env = {
		print = print,
		watch = watch_event
	}

	local watch_env = {
		p = print
	}

	local function watch_cmd(cmd)
		local co = next(ctx_active)
		watch_env._co = co
		if command[cmd] then
			command[cmd](co)
		else
			run_cmd(cmd, watch_env, co, 0)
		end
	end

	local function hook(func,source,cmd,...)
		if source ~= debugger then
			command_f(func,source,cmd,...)
		else
			if cmd == "leave" then
				remove_watch()
				remove_hook(eventDispatch)
			else
				if cmd ~= "" then
					if next(ctx_active) then
						watch_cmd(cmd)
					else
						run_cmd(cmd, env, coroutine.running(),2)
					end
				end
			end
		end
	end

	command_f = replace_upvalue(eventDispatch, HOOK_FUNC, hook)
	print(string.format(":%08x>", serviceCore.self()))
end

return debuggerInit
