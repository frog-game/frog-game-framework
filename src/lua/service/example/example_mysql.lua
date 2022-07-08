local serviceCore = require "serviceCore"
local mysql = require "db.mysql"
local logExt = require "logExt"

serviceCore.start(function()
    local db = mysql.connect({address = "127.0.0.1:3306",user = "root",password = "123456",db = "blueprint"})
    local r = db:query("show databases")
    logExt("example_mysql ",r)

    local r2 = db:query("show databases",true)
    logExt("example_mysql ",r2)

    local r3 = db:query("insert into test(name,level) values('test3',19)")
    logExt("example_mysql ",r3)
    local r4 = db:query("select * from test")
    logExt("example_mysql ",r4)

    local r5,r6 = db:query("select * from test;insert into test(name,level) values('test2077',19)")
    logExt("example_mysql ",r5,r6)

    local r7 = db:query("select * from test")
    logExt("example_mysql ",r7)

    local r8 = db:query("prepare testStmt from 'insert into test(name,level) values(?,?)'")
    logExt("example_mysql ",r8)
    local r9,r10 = db:query( string.format("set @a='%s',@b=%d;execute testStmt using @a,@b","boboTestStmt",9898) )
    logExt("example_mysql ",r9,r10)
    
    serviceCore.exit()
end)