local serviceCore = require "serviceCore"

serviceCore.start(function()
    local t = serviceCore.dnsResolve("www.github.com",false)
    if t then
        for k,v in ipairs(t) do
            serviceCore.log("example_dnsResolve www.github.com:" .. v);
        end
    else
        serviceCore.log("example_dnsResolve www.github.com: error")
    end


    t = serviceCore.dnsResolve("example.com",false,true)
    if t then
        for k,v in ipairs(t) do
            serviceCore.log("example_dnsResolve example.com:" .. v);
        end
    else
        serviceCore.log("example_dnsResolve example.com: error")
    end
    serviceCore.exit()
end)