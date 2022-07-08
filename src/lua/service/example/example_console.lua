local serviceCore = require "serviceCore"

local mode = ...

local command = {}

function command.testCall(s,n,b,f)
	if s ~= "testdata" then
		error("string error")
	end

	if n ~= 543634 then
		error("int error")
	end

	if f ~= 0.0001 then
		return 
	end

	if not b then
		return
	end
	local c = serviceCore.call(serviceCore.getRunningAddress(),"testReply")
	print(c)
	local cc = c+5
	return cc
end

function command.testReply()
	return 5
end

if mode == "call" then
	serviceCore.start(function()
		serviceCore.eventDispatch(serviceCore.eventCall, function(_,cmd,...)
			local f = command[cmd]
			if f then
				serviceCore.reply(f(...))
			else
				error(string.format("unknown command %s", tostring(cmd)))
			end
		end)
		local c = serviceCore.call("exampleCall","testCall","testdata",543634,true,0.0001)
		print(c)
	end)
else
	serviceCore.start(function()
		serviceCore.bindName("exampleCall")
		serviceCore.eventDispatch(serviceCore.eventCall, function(_,cmd,...)
			local f = command[cmd]
			if f then
				serviceCore.reply(f(...))
			else
				error(string.format("unknown command %s", tostring(cmd)))
			end
		end)
	end)
end

