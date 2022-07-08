local serviceCore = require "serviceCore"
local lcrypt = require "lruntime.crypt"

serviceCore.start(function()
    local s = "324ergfol msdl;weqiojlkj>>?>?34234666"
    local d = lcrypt.base64Encode(s)
    local c = lcrypt.base64Decode(d)
    if s ~= c then
        serviceCore.log("example_crypt base64Decode error:".. c)
    end

    local key ="sda23234<?>d"
    d = lcrypt.aesEncrypt(key,s)
    c = lcrypt.aesDecrypt(key,d)

    if s ~= c then
        serviceCore.log("example_crypt aesDecrypt error:".. c)
    end

    local r = lcrypt.randBytes(16)
    serviceCore.log("example_crypt randBytes16:".. lcrypt.base64Encode(r))
    r = lcrypt.randBytes(32)
    serviceCore.log("example_crypt randBytes32:".. lcrypt.base64Encode(r))
    r = lcrypt.randBytes(64)
    serviceCore.log("example_crypt randBytes64:".. lcrypt.base64Encode(r))
    r = lcrypt.randBytes(128)
    serviceCore.log("example_crypt randBytes128:".. lcrypt.base64Encode(r))

    local user = "testuser"
    local password = "testpassword"
    local salt,verifier = lcrypt.srpCreateVerifier(user,password)
    serviceCore.log("example_crypt salt:".. lcrypt.base64Encode(salt))
    serviceCore.log("example_crypt verifer:".. lcrypt.base64Encode(verifier))

	local privKey, pubKey = lcrypt.srpCreateKeyClient()
    serviceCore.log("example_crypt privKey:".. lcrypt.base64Encode(privKey))
    serviceCore.log("example_crypt pubKey:".. lcrypt.base64Encode(pubKey))

    local serverPrivKey, serverPubKey = lcrypt.srpCreateKeyServer(verifier)
    serviceCore.log("example_crypt serverPrivKey:".. lcrypt.base64Encode(serverPrivKey))
    serviceCore.log("example_crypt serverPubKey:".. lcrypt.base64Encode(serverPubKey))

    local serverSessionKey = lcrypt.srpCreateSessionKeyServer(verifier,serverPrivKey,serverPubKey,pubKey)
    serviceCore.log("example_crypt serverSessionKey:".. lcrypt.base64Encode(serverSessionKey))

    local sessionKey = lcrypt.srpCreateSessionKeyClient(user,password,salt,privKey,pubKey,serverPubKey)
    serviceCore.log("example_crypt sessionKey:".. lcrypt.base64Encode(sessionKey))
    if sessionKey == serverSessionKey then
        serviceCore.log("example_crypt srp succ")
    else
        serviceCore.log("example_crypt srp err")
    end

    serviceCore.exit()
end)