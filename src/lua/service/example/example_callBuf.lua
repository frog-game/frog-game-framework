local serviceCore = require "serviceCore"

serviceCore.start(function()
	serviceCore.setMsgCodec({
		decode =  function(msg, sz)
			return msg, sz
		end
		,
		encode = function(msg, sz)
			return msg, sz
		end
	})

	serviceCore.eventDispatch(serviceCore.eventCall, function(_,msg, sz)
		local cmd = serviceCore.cbufferToString(msg, sz)
		serviceCore.log("example_callBuf: recv cmd:" .. cmd );
		serviceCore.replyBuf("example_callBuf reply:".. cmd )
		if cmd == "stop" then
			serviceCore.exit()
		end
	end)
end)