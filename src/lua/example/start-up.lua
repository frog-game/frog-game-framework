local lenv = require "lruntime.env"

lenv.init("data/lua/example/config.lua")

lenv.wait()

lenv.exit()
