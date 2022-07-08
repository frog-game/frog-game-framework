local serviceCore = require "serviceCore"

serviceCore.start(function()
    local log = serviceCore.createService("logService")
    serviceCore.bindServiceName(log, "_log")

    local monitor = serviceCore.createService("monitorService")
    serviceCore.bindServiceName(monitor, "_monitor")

    local localS = serviceCore.createService("localServices")
    serviceCore.bindServiceName(localS, "_localS")

    local console = serviceCore.createService("consoleService")
    serviceCore.call(console, "start", "127.0.0.1:23")

    serviceCore.launch("example/example")

    serviceCore.exit()
end)
