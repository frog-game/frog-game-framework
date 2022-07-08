local serviceCore = require "serviceCore"

serviceCore.start(function()
	serviceCore.log("example_sleep begin test sleep 1000");
	local t = serviceCore.getClockMonotonic()
	serviceCore.sleep(1000)
	serviceCore.log(string.format("example_sleep sleep time %d", serviceCore.getClockMonotonic() - t))

	local co = coroutine.running()

	serviceCore.async(function()
		serviceCore.log("example_sleep wakeup start");
		serviceCore.wakeup(co)
		serviceCore.log("example_sleep wakeup end");
	end)

	serviceCore.log("example_sleep wait start");
	serviceCore.wait(co);
	serviceCore.log("example_sleep wait end");
	serviceCore.log("example_sleep end: test sleep");

end)