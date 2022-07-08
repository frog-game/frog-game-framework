#include "openssl/crypt_t.h"
#include "openssl/sha.h"
#include "openssl/evp.h"
#include "openssl/crypto.h"
#include "openssl/md5.h"
#include "openssl/aes.h"
#include "openssl/rand.h"
#include "openssl/srp.h"
#include "utility_t.h"

static void* customize_crypto_malloc(size_t num, const char* file, int line)
{
    return mem_malloc(num);
}

static void* customize_crypto_realloc(void* str, size_t num, const char* file, int line)
{
    return mem_realloc(str, num);
}

static void customize_crypto_free(void* str, const char* file, int line)
{
    mem_free(str);
}

void crypto_customize_mem_init()
{
    CRYPTO_set_mem_functions(
        customize_crypto_malloc, customize_crypto_realloc, customize_crypto_free);
}

void crypt_sha1(const void* data, size_t len, uint8_t* digest)
{
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    SHA1_Final(digest, &ctx);
}

void crypt_sha224(const void* data, size_t len, uint8_t* digest)
{
    SHA256_CTX ctx;
    SHA224_Init(&ctx);
    SHA224_Update(&ctx, data, len);
    SHA224_Final(digest, &ctx);
}

void crypt_sha256(const void* data, size_t len, uint8_t* digest)
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(digest, &ctx);
}

void crypt_sha384(const void* data, size_t len, uint8_t* digest)
{
    SHA512_CTX ctx;
    SHA384_Init(&ctx);
    SHA384_Update(&ctx, data, len);
    SHA384_Final(digest, &ctx);
}

void crypt_sha512(const void* data, size_t len, uint8_t* digest)
{
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, data, len);
    SHA512_Final(digest, &ctx);
}

int32_t crypt_base64Encode(const uint8_t* data, int32_t n, uint8_t* out)
{
    return EVP_EncodeBlock(out, data, n);
}

int32_t crypt_base64Decode(const uint8_t* data, int32_t n, uint8_t* out)
{
    return EVP_DecodeBlock(out, data, n);
}

void crypt_md5(const void* data, size_t len, uint8_t* digest)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    MD5_Final(digest, &ctx);
}

void crypt_aes_cbc_encrypt(const uint8_t* key, const void* data, size_t len, uint8_t* out)
{
    AES_KEY aes;
    uint8_t ivec[16] = {};
    AES_set_encrypt_key(key, 128, &aes);
    AES_cbc_encrypt(data, out, len, &aes, ivec, AES_ENCRYPT);
}

void crypt_aes_cbc_decrypt(const uint8_t* key, const void* data, size_t len, uint8_t* out)
{
    AES_KEY aes;
    uint8_t ivec[16] = {};
    AES_set_decrypt_key(key, 128, &aes);
    AES_cbc_encrypt(data, out, len, &aes, ivec, AES_DECRYPT);
}

int32_t crypt_rand_bytes(uint8_t* key, int32_t len)
{
    return RAND_bytes(key, len);
}

int32_t crypt_srp_create_verifier(const char* id, const char* user, const char* password,
                                  uint8_t* salt, int32_t* saltLen, uint8_t* verifier,
                                  int32_t* verifierLen)
{
    BIGNUM* s = NULL;
    BIGNUM* v = NULL;

    const SRP_gN* GN = SRP_get_default_gN(id);

    if (!SRP_create_verifier_BN(user, password, &s, &v, GN->N, GN->g)) {
        return -1;
    }

    int32_t n = BN_num_bytes(s);
    if (n > *saltLen) {
        BN_clear_free(s);
        BN_clear_free(v);
        return -1;
    }

    *saltLen = BN_bn2bin(s, salt);

    n = BN_num_bytes(v);
    if (n > *verifierLen) {
        BN_clear_free(s);
        BN_clear_free(v);
        return -1;
    }

    *verifierLen = BN_bn2bin(v, verifier);

    BN_clear_free(s);
    BN_clear_free(v);
    return 0;
}

#define RANDOM_SIZE 32 /* use 256 bits on each side */

int32_t crypt_srp_create_key_server(const char* id, const uint8_t* verifier, int32_t verifierLen,
                                    uint8_t* privKey, int32_t* privKeyLen, uint8_t* pubKey,
                                    int32_t* pubKeyLen)
{
    BIGNUM* priv = NULL;
    BIGNUM* pub  = NULL;
    uint8_t key[RANDOM_SIZE];

    const SRP_gN* GN = SRP_get_default_gN(id);

    BIGNUM* v = BN_bin2bn(verifier, verifierLen, NULL);

    RAND_bytes(key, RANDOM_SIZE);
    priv = BN_bin2bn(key, RANDOM_SIZE, NULL);
    pub  = SRP_Calc_B(priv, GN->N, GN->g, v);
    if (!SRP_Verify_B_mod_N(pub, GN->N)) {
        BN_clear_free(v);
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    int32_t n = BN_num_bytes(priv);
    if (n > *privKeyLen) {
        BN_clear_free(v);
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    *privKeyLen = BN_bn2bin(priv, privKey);

    n = BN_num_bytes(pub);
    if (n > *pubKeyLen) {
        BN_clear_free(v);
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    *pubKeyLen = BN_bn2bin(pub, pubKey);

    BN_clear_free(v);
    BN_clear_free(priv);
    BN_free(pub);
    return 0;
}

int32_t crypt_srp_create_key_client(const char* id, uint8_t* privKey, int32_t* privKeyLen,
                                    uint8_t* pubKey, int32_t* pubKeyLen)
{
    BIGNUM* priv = NULL;
    BIGNUM* pub  = NULL;
    uint8_t key[RANDOM_SIZE];

    const SRP_gN* GN = SRP_get_default_gN(id);

    RAND_bytes(key, RANDOM_SIZE);
    priv = BN_bin2bn(key, RANDOM_SIZE, NULL);
    pub  = SRP_Calc_A(priv, GN->N, GN->g);
    if (!SRP_Verify_A_mod_N(pub, GN->N)) {
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    int32_t n = BN_num_bytes(priv);
    if (n > *privKeyLen) {
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    *privKeyLen = BN_bn2bin(priv, privKey);

    n = BN_num_bytes(pub);
    if (n > *pubKeyLen) {
        BN_clear_free(priv);
        BN_free(pub);
        return -1;
    }

    *pubKeyLen = BN_bn2bin(pub, pubKey);

    BN_clear_free(priv);
    BN_free(pub);
    return 0;
}

int32_t crypt_srp_create_session_key_server(const char* id, const uint8_t* verifier,
                                            int32_t verifierLen, const uint8_t* serverPrivKey,
                                            int32_t serverPrivKeyLen, const uint8_t* serverPubKey,
                                            int32_t serverPubKeyLen, const uint8_t* clientPubKey,
                                            int32_t clientPubKeyLen, uint8_t* sessionKey,
                                            int32_t* sessionKeyLen)
{
    const SRP_gN* GN        = SRP_get_default_gN(id);
    BIGNUM*       v         = BN_bin2bn(verifier, verifierLen, NULL);
    BIGNUM*       priv      = BN_bin2bn(serverPrivKey, serverPrivKeyLen, NULL);
    BIGNUM*       pub       = BN_bin2bn(serverPubKey, serverPubKeyLen, NULL);
    BIGNUM*       clientPub = BN_bin2bn(clientPubKey, clientPubKeyLen, NULL);

    BIGNUM* u = SRP_Calc_u(clientPub, pub, GN->N);
    BIGNUM* K = SRP_Calc_server_key(clientPub, v, u, priv, GN->N);

    int32_t n = BN_num_bytes(K);
    if (n > *sessionKeyLen) {
        BN_clear_free(priv);
        BN_clear_free(pub);
        BN_clear_free(clientPub);
        BN_clear_free(K);
        BN_clear_free(v);
        BN_free(u);
        return -1;
    }
    *sessionKeyLen = BN_bn2bin(K, sessionKey);

    BN_clear_free(priv);
    BN_clear_free(pub);
    BN_clear_free(clientPub);
    BN_clear_free(K);
    BN_clear_free(v);
    BN_free(u);

    return 0;
}

int32_t crypt_srp_create_session_key_client(const char* id, const char* user, const char* password,
                                            const uint8_t* salt, int32_t saltLen,
                                            const uint8_t* clientPrivKey, int32_t clientPrivKeyLen,
                                            const uint8_t* clientPubKey, int32_t clientPubKeyLen,
                                            const uint8_t* serverPubKey, int32_t serverPubKeyLen,
                                            uint8_t* sessionKey, int32_t* sessionKeyLen)
{
    const SRP_gN* GN        = SRP_get_default_gN(id);
    BIGNUM*       s         = BN_bin2bn(salt, saltLen, NULL);
    BIGNUM*       priv      = BN_bin2bn(clientPrivKey, clientPrivKeyLen, NULL);
    BIGNUM*       pub       = BN_bin2bn(clientPubKey, clientPubKeyLen, NULL);
    BIGNUM*       serverPub = BN_bin2bn(serverPubKey, serverPubKeyLen, NULL);

    BIGNUM* u = SRP_Calc_u(pub, serverPub, GN->N);
    BIGNUM* x = SRP_Calc_x(s, user, password);
    BIGNUM* K = SRP_Calc_client_key(GN->N, serverPub, GN->g, x, priv, u);

    int32_t n = BN_num_bytes(K);
    if (n > *sessionKeyLen) {
        BN_clear_free(priv);
        BN_clear_free(pub);
        BN_clear_free(serverPub);
        BN_clear_free(K);
        BN_clear_free(s);
        BN_free(u);
        return -1;
    }
    *sessionKeyLen = BN_bn2bin(K, sessionKey);

    BN_clear_free(priv);
    BN_clear_free(pub);
    BN_clear_free(serverPub);
    BN_clear_free(K);
    BN_clear_free(s);
    BN_free(u);

    return 0;
}
