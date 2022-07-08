local serviceCore = require "serviceCore"

serviceCore.start(function()
	serviceCore.log("example_timer test timer 10 500");
	local count = 1
	local t1 = serviceCore.getClockMonotonic()
	local fnOnTriggered = function ()
		local t2 = serviceCore.getClockMonotonic()
		serviceCore.log(string.format("example_timer runEvery: %d, time: %d", count, t2 - t1))
		t1 = t2;
		count = count + 1
		if count > 10 then
			serviceCore.async(serviceCore.exit)
			return false;
		else
			return true;
		end
	end
	serviceCore.runEvery(500,fnOnTriggered);
	local t3 = serviceCore.getClockMonotonic()
	serviceCore.timeout(1000,function ()
		local t4 = serviceCore.getClockMonotonic()
		serviceCore.log(string.format("example_timer timeout time: %d", t4 - t3))
	end)

end)
