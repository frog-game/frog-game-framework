local serviceCore = require "serviceCore"
local redis = require "db.redis"
local logExt = require "logExt"

local function publish()
    local db = redis.connect({address = "127.0.0.1:6379",db=0})
    if db then
        serviceCore.log("example_redis redis connect")
    else
        return
    end
	db:subscribe("channel")
	db:psubscribe("publish.*")
	while true do
        logExt("example_redis ",db:message())
	end
end

serviceCore.start(function()
    serviceCore.async(publish)
    
    local db = redis.connect({address = "127.0.0.1:6379",db=0})
    if db then
        serviceCore.log("example_redis connect")
    else
        serviceCore.log("example_redis connect error")
        serviceCore.exit()
        return
    end

    db:del("setH")
	for i=1,10 do
		db:hset("setH",i,i)
	end
    local r = db:hvals("setH")
    logExt("example_redis ",r)
    
    local s = db:get("runoobError")
    logExt("example_redis ",s)
    s = db:set("runoob","hello")
    logExt("example_redis ",s)
    serviceCore.log(db:get("runoob"))

    local cmds = {}
    cmds[1] = "get runoob"
    cmds[2] = "hvals setH"
    logExt("example_redis ",db:cmdPipeline(cmds))

    db:publish("channel","test")
    db:publish("channel","test2")
    db:publish("channel","test3")
    db:publish("publish.test","test")
    
    serviceCore.exit()
end)