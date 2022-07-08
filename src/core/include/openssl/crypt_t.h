#pragma once

// type
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "platform_t.h"

// openssl crypto mem
frCore_API void crypto_customize_mem_init();

// sha
#define DEF_SHA1_DIGEST_LENGTH 20
#define DEF_SHA224_DIGEST_LENGTH 28
#define DEF_SHA256_DIGEST_LENGTH 32
#define DEF_SHA384_DIGEST_LENGTH 48
#define DEF_SHA512_DIGEST_LENGTH 64

frCore_API void crypt_sha1(const void* data, size_t len, uint8_t* digest);

frCore_API void crypt_sha224(const void* data, size_t len, uint8_t* digest);

frCore_API void crypt_sha256(const void* data, size_t len, uint8_t* digest);

frCore_API void crypt_sha384(const void* data, size_t len, uint8_t* digest);

frCore_API void crypt_sha512(const void* data, size_t len, uint8_t* digest);

// base64
frCore_API int32_t crypt_base64Encode(const uint8_t* data, int32_t n, uint8_t* out);

frCore_API int32_t crypt_base64Decode(const uint8_t* data, int32_t n,
                                      uint8_t* out);   // Need to remove the following 0

// md5
#define DEF_MD5_DIGEST_LENGTH 16

frCore_API void crypt_md5(const void* data, size_t len, uint8_t* digest);

// aes
#define DEF_AES_BLOCK_SIZE 16

frCore_API void crypt_aes_cbc_encrypt(const uint8_t* key, const void* data, size_t len,
                                      uint8_t* out);

frCore_API void crypt_aes_cbc_decrypt(const uint8_t* key, const void* data, size_t len,
                                      uint8_t* out);

// rand
frCore_API int32_t crypt_rand_bytes(uint8_t* key, int32_t len);

// srp
frCore_API int32_t crypt_srp_create_verifier(const char* id, const char* user, const char* password,
                                             uint8_t* salt, int32_t* saltLen, uint8_t* verifier,
                                             int32_t* verifierLen);

frCore_API int32_t crypt_srp_create_key_server(const char* id, const uint8_t* verifier,
                                               int32_t verifierLen, uint8_t* privKey,
                                               int32_t* privKeyLen, uint8_t* pubKey,
                                               int32_t* pubKeyLen);

frCore_API int32_t crypt_srp_create_key_client(const char* id, uint8_t* privKey,
                                               int32_t* privKeyLen, uint8_t* pubKey,
                                               int32_t* pubKeyLen);

frCore_API int32_t crypt_srp_create_session_key_server(
    const char* id, const uint8_t* verifier, int32_t verifierLen, const uint8_t* serverPrivKey,
    int32_t serverPrivKeyLen, const uint8_t* serverPubKey, int32_t serverPubKeyLen,
    const uint8_t* clientPubKey, int32_t clientPubKeyLen, uint8_t* sessionKey,
    int32_t* sessionKeyLen);

frCore_API int32_t crypt_srp_create_session_key_client(
    const char* id, const char* user, const char* password, const uint8_t* salt, int32_t saltLen,
    const uint8_t* clientPrivKey, int32_t clientPrivKeyLen, const uint8_t* clientPubKey,
    int32_t clientPubKeyLen, const uint8_t* serverPubKey, int32_t serverPubKeyLen,
    uint8_t* sessionKey, int32_t* sessionKeyLen);
