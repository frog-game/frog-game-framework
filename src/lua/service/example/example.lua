local serviceCore = require "serviceCore"


local function example_log()
	serviceCore.log("testLog: test log string")

    local logExt = require "logExt"
	local t = { testTable = {3,4,5},test2 = "test log string"}
	logExt("testLog",t)
end

local function example_crypt()
	serviceCore.launch("example/example_crypt")
end

local function example_sleep()
	serviceCore.launch("example/example_sleep")
end

local function example_timer()
	serviceCore.launch("example/example_timer")
end

local function example_exit()
	serviceCore.launch("example/example_exit","exit")
	local service = serviceCore.launch("example/example_exit")
    serviceCore.command(service,"stop")
end

local function example_command()
	local service = serviceCore.launch("example/example_command")
	serviceCore.command(service,"set","data2",78)
	local v1 = serviceCore.callCommand(service,"get","data2")
	serviceCore.log("example_command get value:".. v1 );

	serviceCore.callCommand(service,"set","data1",41)
	local v2 = serviceCore.callCommand(service,"get","data1")
	serviceCore.log("example_command get value:".. v2 );
	local v3 = serviceCore.callCommand(service,"add",89,56)
	serviceCore.log("example_command: add value 89+56 =".. v3);
	serviceCore.callCommand(service,"stop")
end

local function example_call()
	local service = serviceCore.launch("example/example_call")
	serviceCore.call(service,"set","data1",41)
	local v2 = serviceCore.call(service,"get","data1")
	serviceCore.log("example_call get value:".. v2 );
	local v3 = serviceCore.call(service,"add",89,56)
	serviceCore.log("example_call: add value 89+56 =".. v3);
	serviceCore.call(service,"stop")
end

local function example_callBuf()
	local service = serviceCore.launch("example/example_callBuf")
	local s,l = serviceCore.callBuf(service,"test call")
	serviceCore.log("example_callBuf: reply:".. serviceCore.cbufferToString(s,l) );
    serviceCore.callBuf(service,"stop")
end

local function example_queueLock()
	local service = serviceCore.launch("example/example_queueLock")
	serviceCore.command(service,"set","data1",41)
	serviceCore.command(service,"set","data1",98)
	serviceCore.command(service,"set","data1",39)
	serviceCore.command(service,"set","data1",108)
	local v2 = serviceCore.callCommand(service,"get","data1")
	serviceCore.log("example_queueLock get value:108 = ".. v2 );
    serviceCore.callCommand(service,"stop")
end

local function example_channel_tcp()
	local ltpack = require "lruntime.tpack"
	local service = serviceCore.launch("example/example_channel")
	serviceCore.callCommand(service,"start","127.0.0.1:8000",false)
	local channelID = serviceCore.connect("127.0.0.1:8000",10000);
	if channelID then
		serviceCore.remoteBind(channelID,false,true,ltpack.codecHandle())
		serviceCore.log("example_channel tcp connect success source ID:" .. channelID);
		local s = serviceCore.call(channelID,"test","hello net")
		if s then
			serviceCore.log("example_channel tcp testListenPort recv:" .. s);
		end
        serviceCore.send(service,"set","data2",78)
        local v1 = serviceCore.call(service,"get","data2")
        serviceCore.log("example_command get value:".. v1 );
        
        serviceCore.call(channelID,"set","data1",41)
        local v2 = serviceCore.call(channelID,"get","data1")
        serviceCore.log("example_channel tcp get value:".. v2 );
        local v3 = serviceCore.call(channelID,"add",89,56)
        serviceCore.log("example_channel tcp add value 89+56 =".. v3);

		serviceCore.remoteClose(channelID)
		channelID = nil
	else
		serviceCore.log("example_channel tcp: connect error");
	end
    serviceCore.callCommand(service,"stop")
end

local function example_channel_udp()
	local ltpack = require "lruntime.tpack"
	local service = serviceCore.launch("example/example_channel")
	serviceCore.callCommand(service,"start","127.0.0.1:8001",true)
	local channelID = serviceCore.connect("127.0.0.1:8001",10000,true);
	if channelID then
		serviceCore.remoteBind(channelID,false,false,ltpack.codecHandle())
		serviceCore.log("example_channel udp connect success source ID:" .. channelID);

        serviceCore.remoteWrite(channelID,"connect")

		serviceCore.sleep(1000)

		local s = serviceCore.call(channelID,"test","hello net")
		if s then
			serviceCore.log("example_channel udp testListenPort recv:" .. s);
		end
        serviceCore.send(service,"set","data2",78)
        local v1 = serviceCore.call(service,"get","data2")
        serviceCore.log("example_command get value:".. v1 );
        
        serviceCore.call(channelID,"set","data1",41)
        local v2 = serviceCore.call(channelID,"get","data1")
        serviceCore.log("example_channel udp get value:".. v2 );
        local v3 = serviceCore.call(channelID,"add",89,56)
        serviceCore.log("example_channel udp add value 89+56 =".. v3);

		serviceCore.remoteClose(channelID)
		channelID = nil
	else
		serviceCore.log("example_channel udp: connect error");
	end
    serviceCore.callCommand(service,"stop")
end


local function example_dnsResolve()
	serviceCore.launch("example/example_dnsResolve")
end

local function example_http()
	serviceCore.launch("example/example_httpResponse")
	serviceCore.launch("example/example_httpRequest")
end

local function example_mysql()
	serviceCore.launch("example/example_mysql")
end

local function example_redis()
	local test = serviceCore.launch("example/example_redis")
end

serviceCore.start(function()
	example_log()
    example_crypt()
	example_sleep()
    example_timer()
	example_exit()
    example_command()
	example_call()
	example_callBuf()
    example_queueLock()
    example_channel_tcp()
	example_channel_udp()
	example_dnsResolve()
	example_http()
	--testMysql()
	--testRedis()
	serviceCore.exit()
end)