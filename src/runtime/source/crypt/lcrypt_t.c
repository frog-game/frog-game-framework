

#include "crypt/lcrypt_t.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "openssl/crypt_t.h"

static int32_t lsha1(lua_State* L)
{
    size_t      sz      = 0;
    const void* pBuffer = luaL_checklstring(L, 1, &sz);
    uint8_t     digest[DEF_SHA1_DIGEST_LENGTH];
    crypt_sha1(pBuffer, sz, digest);
    lua_pushlstring(L, (const char*)digest, DEF_SHA1_DIGEST_LENGTH);
    return 1;
}

static int32_t lmd5(lua_State* L)
{
    size_t      sz      = 0;
    const void* pBuffer = luaL_checklstring(L, 1, &sz);
    uint8_t     digest[DEF_MD5_DIGEST_LENGTH];
    crypt_md5(pBuffer, sz, digest);
    lua_pushlstring(L, (const char*)digest, DEF_MD5_DIGEST_LENGTH);
    return 1;
}

static int32_t lbase64_encode(lua_State* L)
{
    size_t         sz             = 0;
    const uint8_t* pBuffer        = (const uint8_t*)luaL_checklstring(L, 1, &sz);
    int32_t        iEncodeLength  = sz * 2;
    uint8_t        tmpBuffer[128] = {};
    uint8_t*       pEncodeBuffer  = tmpBuffer;
    if (iEncodeLength > 128) {
        pEncodeBuffer = (uint8_t*)lua_newuserdatauv(L, iEncodeLength, 0);
    }
    int32_t iLength = crypt_base64Encode(pBuffer, sz, pEncodeBuffer);
    if (iLength < 0) {
        return luaL_error(L, "base64encode error");
    }
    lua_pushlstring(L, (const char*)pEncodeBuffer, iLength);
    return 1;
}

static int32_t lbase64_decode(lua_State* L)
{
    size_t         sz      = 0;
    const uint8_t* pBuffer = (const uint8_t*)luaL_checklstring(L, 1, &sz);

    int32_t  iDecodeLength  = (sz / 4) * 3;
    uint8_t  tmpBuffer[128] = {};
    uint8_t* pDecodeBuffer  = tmpBuffer;
    if (iDecodeLength > 128) {
        pDecodeBuffer = (uint8_t*)lua_newuserdatauv(L, iDecodeLength, 0);
    }

    int32_t iLength = crypt_base64Decode(pBuffer, sz, pDecodeBuffer);
    if (iLength < 0) {
        return luaL_error(L, "base64decode error");
    }

    int32_t i = 0;
    while (pBuffer[--sz] == '=') {
        --iLength;
        if (++i > 2) {
            return luaL_error(L, "base64decode error");
        }
    }

    lua_pushlstring(L, (const char*)pDecodeBuffer, iLength);
    return 1;
}

static int32_t laes_encrypt(lua_State* L)
{
    const uint8_t* szKey   = (const uint8_t*)luaL_checkstring(L, 1);
    size_t         sz      = 0;
    const uint8_t* pBuffer = (const uint8_t*)luaL_checklstring(L, 2, &sz);

    size_t nEncryptLength =
        (sz % DEF_AES_BLOCK_SIZE) == 0 ? sz : sz + (DEF_AES_BLOCK_SIZE - (sz % DEF_AES_BLOCK_SIZE));

    uint8_t  tmpBuffer[128] = {};
    uint8_t* pEncryptBuffer = tmpBuffer;
    if (nEncryptLength > 128) {
        pEncryptBuffer = (uint8_t*)lua_newuserdatauv(L, nEncryptLength, 0);
    }
    crypt_aes_cbc_encrypt(szKey, pBuffer, sz, pEncryptBuffer);
    lua_pushlstring(L, (const char*)pEncryptBuffer, nEncryptLength);
    return 1;
}

static int32_t laes_decrypt(lua_State* L)
{
    const uint8_t* szKey   = (const uint8_t*)luaL_checkstring(L, 1);
    size_t         sz      = 0;
    const uint8_t* pBuffer = (const uint8_t*)luaL_checklstring(L, 2, &sz);

    uint8_t  tmpBuffer[128] = {};
    uint8_t* pDecryptBuffer = tmpBuffer;
    if (sz > 128) {
        pDecryptBuffer = (uint8_t*)lua_newuserdatauv(L, sz, 0);
    }

    crypt_aes_cbc_decrypt(szKey, pBuffer, sz, pDecryptBuffer);
    lua_pushstring(L, (const char*)pDecryptBuffer);
    return 1;
}

static int32_t lrand_bytes(lua_State* L)
{
    int32_t  len           = (int32_t)luaL_checkinteger(L, 1);
    uint8_t  tmpBuffer[64] = {};
    uint8_t* pBuffer       = tmpBuffer;
    if (len > 64) {
        pBuffer = (uint8_t*)lua_newuserdatauv(L, len, 0);
    }
    crypt_rand_bytes(pBuffer, len);
    lua_pushlstring(L, (const char*)pBuffer, len);
    return 1;
}

#define SRP_RANDOM_SALT_LEN 20

static int32_t lsrp_create_verifier(lua_State* L)
{
    const char* szUser     = luaL_checkstring(L, 1);
    const char* szPassword = luaL_checkstring(L, 2);
    uint8_t     salt[SRP_RANDOM_SALT_LEN];
    int32_t     saltLen = SRP_RANDOM_SALT_LEN;
    uint8_t     verifier[256];
    int32_t     verifierLen = 256;
    if (crypt_srp_create_verifier(
            "1024", szUser, szPassword, salt, &saltLen, verifier, &verifierLen) == 0) {
        lua_pushlstring(L, (const char*)salt, saltLen);
        lua_pushlstring(L, (const char*)verifier, verifierLen);
        return 2;
    }
    return 0;
}

static int32_t lsrp_create_key_server(lua_State* L)
{
    size_t         sz       = 0;
    const uint8_t* verifier = (const uint8_t*)luaL_checklstring(L, 1, &sz);

    uint8_t privKey[32];
    int32_t privKeyLen = 32;
    uint8_t pubKey[256];
    int32_t pubKeyLen = 256;

    if (crypt_srp_create_key_server(
            "1024", verifier, (int32_t)sz, privKey, &privKeyLen, pubKey, &pubKeyLen) == 0) {
        lua_pushlstring(L, (const char*)privKey, privKeyLen);
        lua_pushlstring(L, (const char*)pubKey, pubKeyLen);
        return 2;
    }
    return 0;
}

static int32_t lsrp_create_key_client(lua_State* L)
{
    uint8_t privKey[32];
    int32_t privKeyLen = 32;
    uint8_t pubKey[256];
    int32_t pubKeyLen = 256;

    if (crypt_srp_create_key_client("1024", privKey, &privKeyLen, pubKey, &pubKeyLen) == 0) {
        lua_pushlstring(L, (const char*)privKey, privKeyLen);
        lua_pushlstring(L, (const char*)pubKey, pubKeyLen);
        return 2;
    }
    return 0;
}

static int32_t lsrp_create_session_key_server(lua_State* L)
{
    size_t         sz       = 0;
    const uint8_t* verifier = (const uint8_t*)luaL_checklstring(L, 1, &sz);

    size_t         serverPrivKeyLen = 0;
    const uint8_t* serverPrivKey    = (const uint8_t*)luaL_checklstring(L, 2, &serverPrivKeyLen);

    size_t         serverPubKeyLen = 0;
    const uint8_t* serverPubKey    = (const uint8_t*)luaL_checklstring(L, 3, &serverPubKeyLen);

    size_t         clientPubKeyLen = 0;
    const uint8_t* clientPubKey    = (const uint8_t*)luaL_checklstring(L, 4, &clientPubKeyLen);

    uint8_t sessionKey[256];
    int32_t sessionKeyLen = 256;

    if (crypt_srp_create_session_key_server("1024",
                                            verifier,
                                            (int32_t)sz,
                                            serverPrivKey,
                                            (int32_t)serverPrivKeyLen,
                                            serverPubKey,
                                            (int32_t)serverPubKeyLen,
                                            clientPubKey,
                                            (int32_t)clientPubKeyLen,
                                            sessionKey,
                                            &sessionKeyLen) == 0) {
        lua_pushlstring(L, (const char*)sessionKey, sessionKeyLen);
        return 1;
    }
    return 0;
}

static int32_t lsrp_create_session_key_client(lua_State* L)
{
    const char* szUser     = luaL_checkstring(L, 1);
    const char* szPassword = luaL_checkstring(L, 2);

    size_t         sz   = 0;
    const uint8_t* salt = (const uint8_t*)luaL_checklstring(L, 3, &sz);

    size_t         clientPrivKeyLen = 0;
    const uint8_t* clientPrivKey    = (const uint8_t*)luaL_checklstring(L, 4, &clientPrivKeyLen);

    size_t         clientPubKeyLen = 0;
    const uint8_t* clientPubKey    = (const uint8_t*)luaL_checklstring(L, 5, &clientPubKeyLen);

    size_t         serverPubKeyLen = 0;
    const uint8_t* serverPubKey    = (const uint8_t*)luaL_checklstring(L, 6, &serverPubKeyLen);

    uint8_t sessionKey[256];
    int32_t sessionKeyLen = 256;

    if (crypt_srp_create_session_key_client("1024",
                                            szUser,
                                            szPassword,
                                            salt,
                                            (int32_t)sz,
                                            clientPrivKey,
                                            (int32_t)clientPrivKeyLen,
                                            clientPubKey,
                                            (int32_t)clientPubKeyLen,
                                            serverPubKey,
                                            (int32_t)serverPubKeyLen,
                                            sessionKey,
                                            &sessionKeyLen) == 0) {
        lua_pushlstring(L, (const char*)sessionKey, sessionKeyLen);
        return 1;
    }
    return 0;
}

int32_t luaopen_lruntime_crypt(struct lua_State* L)
{
#ifdef luaL_checkversion
    luaL_checkversion(L);
#endif
    luaL_Reg lualib_crypt[] = {{"sha1", lsha1},
                               {"md5", lmd5},
                               {"base64Encode", lbase64_encode},
                               {"base64Decode", lbase64_decode},
                               {"aesEncrypt", laes_encrypt},
                               {"aesDecrypt", laes_decrypt},
                               {"randBytes", lrand_bytes},
                               {"srpCreateVerifier", lsrp_create_verifier},
                               {"srpCreateKeyServer", lsrp_create_key_server},
                               {"srpCreateKeyClient", lsrp_create_key_client},
                               {"srpCreateSessionKeyServer", lsrp_create_session_key_server},
                               {"srpCreateSessionKeyClient", lsrp_create_session_key_client},
                               {NULL, NULL}};
    luaL_newlib(L, lualib_crypt);
    return 1;
}
