local serviceCore = require "serviceCore"
local lenv = require "lruntime.env"

serviceCore.start(function()
	serviceCore.eventDispatch(serviceCore.eventText, function(source)
		serviceCore.log(string.format("monitor service exception serviceId:%08x",source))
	end)
	lenv.monitorStart(serviceCore.self(),5000)
end)