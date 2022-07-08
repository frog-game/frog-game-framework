#include "openssl/ssl_t.h"
#include "log_t.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/bn.h"
#include "openssl/conf.h"
#include "openssl/crypto.h"
#include "openssl/dh.h"
#include "openssl/engine.h"
#include "openssl/evp.h"
#include "openssl/hmac.h"
#include "openssl/ocsp.h"
#include "openssl/rand.h"
#include "openssl/rsa.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"
#include <stdatomic.h>
#include <string.h>

static int32_t s_iConnectionIndex        = -1;
static int32_t s_iConfIndex              = -1;
static int32_t s_iSessionCacheIndex      = -1;
static int32_t s_iSessionTicketKeysIndex = -1;
static int32_t s_iCertificateIndex       = -1;
static int32_t s_iNextCertificateIndex   = -1;
static int32_t s_iStaplingIndex          = -1;

bool sslInit()
{
#if OPENSSL_VERSION_NUMBER >= 0x10100003L

    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL) == 0) {
        return false;
    }
    ERR_clear_error();

#else
    OPENSSL_config(NULL);

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif

    s_iConnectionIndex = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iConnectionIndex == -1) {
        return false;
    }

    s_iConfIndex = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iConfIndex == -1) {
        return false;
    }

    s_iSessionCacheIndex = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iSessionCacheIndex == -1) {
        return false;
    }

    s_iSessionTicketKeysIndex = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iSessionTicketKeysIndex == -1) {
        return false;
    }

    s_iCertificateIndex = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iCertificateIndex == -1) {
        return false;
    }

    s_iNextCertificateIndex = X509_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    if (s_iNextCertificateIndex == -1) {
        return false;
    }

    s_iStaplingIndex = X509_get_ex_new_index(0, NULL, NULL, NULL, NULL);

    if (s_iStaplingIndex == -1) {
        return false;
    }
    return true;
}

void sslExit()
{
#if OPENSSL_VERSION_NUMBER < 0x10100003L
    EVP_cleanup();
#    ifndef OPENSSL_NO_ENGINE
    ENGINE_cleanup();
#    endif
#endif
}

struct sslContext_s
{
    SSL_CTX*   pCtx;
    atomic_int iRefCount;
};

sslContext_tt* createSSLContext(uint32_t uiProtocols, void* pData)
{
    sslContext_tt* pSSLContext = mem_malloc(sizeof(sslContext_tt));
    atomic_init(&pSSLContext->iRefCount, 1);
    pSSLContext->pCtx = SSL_CTX_new(SSLv23_method());

    if (pSSLContext->pCtx == NULL) {
        mem_free(pSSLContext);
        return NULL;
    }

    if (SSL_CTX_set_ex_data(pSSLContext->pCtx, s_iConfIndex, pData) == 0) {
        mem_free(pSSLContext);
        return NULL;
    }

    if (SSL_CTX_set_ex_data(pSSLContext->pCtx, s_iCertificateIndex, NULL) == 0) {
        mem_free(pSSLContext);
        return NULL;
    }

#ifdef SSL_OP_MICROSOFT_SESS_ID_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_MICROSOFT_SESS_ID_BUG);
#endif

#ifdef SSL_OP_NETSCAPE_CHALLENGE_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NETSCAPE_CHALLENGE_BUG);
#endif

#ifdef SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
#endif

#ifdef SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);
#endif

#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif

#ifdef SSL_OP_SSLEAY_080_CLIENT_DH_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
#endif

#ifdef SSL_OP_TLS_D5_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_TLS_D5_BUG);
#endif

#ifdef SSL_OP_TLS_BLOCK_PADDING_BUG
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_TLS_BLOCK_PADDING_BUG);
#endif

#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif

    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_SINGLE_DH_USE);

#if OPENSSL_VERSION_NUMBER >= 0x009080dfL
    /* only in 0.9.8m+ */
    SSL_CTX_clear_options(pSSLContext->pCtx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1);
#endif

    if (!(uiProtocols & DEF_SSL_SSLv2)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_SSLv2);
    }
    if (!(uiProtocols & DEF_SSL_SSLv3)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_SSLv3);
    }
    if (!(uiProtocols & DEF_SSL_TLSv1)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1);
    }
#ifdef SSL_OP_NO_TLSv1_1
    SSL_CTX_clear_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_1);
    if (!(uiProtocols & DEF_SSL_TLSv1_1)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_1);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    SSL_CTX_clear_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_2);
    if (!(uiProtocols & DEF_SSL_TLSv1_2)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_2);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_3
    SSL_CTX_clear_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_3);
    if (!(uiProtocols & DEF_SSL_TLSv1_3)) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_TLSv1_3);
    }
#endif

#ifdef SSL_CTX_set_min_proto_version
    SSL_CTX_set_min_proto_version(pSSLContext->pCtx, 0);
    SSL_CTX_set_max_proto_version(pSSLContext->pCtx, TLS1_2_VERSION);
#endif

#ifdef TLS1_3_VERSION
    SSL_CTX_set_min_proto_version(pSSLContext->pCtx, 0);
    SSL_CTX_set_max_proto_version(pSSLContext->pCtx, TLS1_3_VERSION);
#endif

#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_OP_NO_ANTI_REPLAY
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_ANTI_REPLAY);
#endif

#ifdef SSL_OP_NO_CLIENT_RENEGOTIATION
    SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_NO_CLIENT_RENEGOTIATION);
#endif

#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(pSSLContext->pCtx, SSL_MODE_RELEASE_BUFFERS);
#endif

#ifdef SSL_MODE_NO_AUTO_CHAIN
    SSL_CTX_set_mode(pSSLContext->pCtx, SSL_MODE_NO_AUTO_CHAIN);
#endif
    SSL_CTX_set_read_ahead(pSSLContext->pCtx, 1);

    // SSL_CTX_set_info_callback(pSSLContext->pCtx , ngx_ssl_info_callback);

    return pSSLContext;
}

void sslContext_addref(sslContext_tt* pSSLContext)
{
    atomic_fetch_add(&(pSSLContext->iRefCount), 1);
}

void sslContext_release(sslContext_tt* pSSLContext)
{
    if (atomic_fetch_sub(&(pSSLContext->iRefCount), 1) == 1) {
        X509* pCert = SSL_CTX_get_ex_data(pSSLContext->pCtx, s_iCertificateIndex);
        while (pCert) {
            X509* pNext = X509_get_ex_data(pCert, s_iNextCertificateIndex);
            X509_free(pCert);
            pCert = pNext;
        }
        SSL_CTX_free(pSSLContext->pCtx);
        mem_free(pSSLContext);
    }
}

static X509* ssl_load_certificate(const char* szCert, size_t nCertLength, STACK_OF(X509) * *ppChain)
{
    BIO* pBio;
    if (strncmp(szCert, "data:", sizeof("data:") - 1) == 0) {
        pBio = BIO_new_mem_buf(szCert + sizeof("data:") - 1, nCertLength - (sizeof("data:") - 1));
        if (pBio == NULL) {
            Log(eLog_error, "BIO_new_mem_buf error");
            return NULL;
        }
    }
    else {
        pBio = BIO_new_file(szCert, "r");
        if (pBio == NULL) {
            Log(eLog_error, "BIO_new_file error");
            return NULL;
        }
    }

    X509* pX509 = PEM_read_bio_X509_AUX(pBio, NULL, NULL, NULL);
    if (pX509 == NULL) {
        Log(eLog_error, "PEM_read_bio_X509_AUX error");
        BIO_free(pBio);
        return NULL;
    }

    *ppChain = sk_X509_new_null();
    if (*ppChain == NULL) {
        Log(eLog_error, "sk_X509_new_null error");
        BIO_free(pBio);
        X509_free(pX509);
        return NULL;
    }

    X509*         pTemp;
    unsigned long n;
    for (;;) {
        pTemp = PEM_read_bio_X509(pBio, NULL, NULL, NULL);
        if (pTemp == NULL) {
            n = ERR_peek_last_error();

            if (ERR_GET_LIB(n) == ERR_LIB_PEM && ERR_GET_REASON(n) == PEM_R_NO_START_LINE) {
                ERR_clear_error();
                break;
            }

            Log(eLog_error, "PEM_read_bio_X509 error");
            BIO_free(pBio);
            X509_free(pX509);
            sk_X509_pop_free(*ppChain, X509_free);
            return NULL;
        }

        if (sk_X509_push(*ppChain, pTemp) == 0) {
            Log(eLog_error, "sk_X509_push error");
            BIO_free(pBio);
            X509_free(pX509);
            sk_X509_pop_free(*ppChain, X509_free);
            return NULL;
        }
    }

    BIO_free(pBio);
    return pX509;
}

static int32_t ssl_password_callback(char* pBuffer, int32_t iSize, int32_t iRWflag, void* pUserdata)
{
    char* szPwd = pUserdata;

    if (iRWflag) {
        Log(eLog_error, "ssl_password_callback error");
        return 0;
    }

    if (szPwd == NULL) {
        return 0;
    }
    size_t nLength = strlen(szPwd);
    if (nLength > (size_t)iSize) {
        Log(eLog_error, "password is truncated to %d bytes", iSize);
        return 0;
    }
    memcpy(pBuffer, szPwd, nLength);
    return nLength;
}

static EVP_PKEY* ssl_load_certificate_key(char* szKey, size_t nKeyLength, const char* szPassword)
{
    EVP_PKEY* pKey;
    BIO*      pBio;
    if (strncmp(szKey, "engine:", sizeof("engine:") - 1) == 0) {
        char* p      = szKey + sizeof("engine:") - 1;
        char* szLast = strchr(p, ':');

        if (szLast == NULL) {
            Log(eLog_error, "ssl_load_certificate_key(%s) error", szKey);
            return NULL;
        }

        *((uint8_t*)szLast) = '\0';

        ENGINE* pEngine = ENGINE_by_id(p);

        if (pEngine == NULL) {
            Log(eLog_error, "ENGINE_by_id(%s) error", p);
            *((uint8_t*)szLast) = ':';
            return NULL;
        }
        *((uint8_t*)szLast) = ':';

        pKey = ENGINE_load_private_key(pEngine, szLast + 1, 0, 0);
        if (pKey == NULL) {
            Log(eLog_error, "ENGINE_load_private_key error:%s", szKey);
            ENGINE_free(pEngine);
            return NULL;
        }

        ENGINE_free(pEngine);
        return pKey;
    }

    if (strncmp(szKey, "data:", sizeof("data:") - 1) == 0) {
        pBio = BIO_new_mem_buf(szKey + sizeof("data:") - 1, nKeyLength - (sizeof("data:") - 1));
        if (pBio == NULL) {
            Log(eLog_error, "BIO_new_mem_buf error:%s", szKey);
            return NULL;
        }
    }
    else {
        pBio = BIO_new_file(szKey, "r");
        if (pBio == NULL) {
            Log(eLog_error, "BIO_new_file error:%s", szKey);
            return NULL;
        }
    }
    const char*      szPwd = szPassword;
    pem_password_cb* cb    = NULL;
    if (szPassword) {
        cb = ssl_password_callback;
    }
    pKey = PEM_read_bio_PrivateKey(pBio, NULL, cb, (void*)szPwd);
    if (pKey == NULL) {
        Log(eLog_error, "PEM_read_bio_PrivateKey error:%s", szKey);
        BIO_free(pBio);
        return NULL;
    }
    BIO_free(pBio);
    return pKey;
}

bool sslContext_loadCertificate(sslContext_tt* pSSLContext, const char* szCert, size_t nCertLength,
                                char* szKey, size_t nKeyLength, const char* szPassword)
{
    STACK_OF(X509)* pChain = NULL;

    X509* pX509 = ssl_load_certificate(szCert, nCertLength, &pChain);
    if (pX509 == NULL) {
        return false;
    }

    if (SSL_CTX_use_certificate(pSSLContext->pCtx, pX509) == 0) {
        Log(eLog_error, "SSL_CTX_use_certificate(%s) error", szCert);
        X509_free(pX509);
        sk_X509_pop_free(pChain, X509_free);
        return false;
    }

    if (X509_set_ex_data(pX509,
                         s_iNextCertificateIndex,
                         SSL_CTX_get_ex_data(pSSLContext->pCtx, s_iCertificateIndex)) == 0) {
        Log(eLog_error, "X509_set_ex_data error");
        X509_free(pX509);
        sk_X509_pop_free(pChain, X509_free);
        return false;
    }

    if (SSL_CTX_set_ex_data(pSSLContext->pCtx, s_iCertificateIndex, pX509) == 0) {
        Log(eLog_error, "SSL_CTX_set_ex_data error");
        X509_free(pX509);
        sk_X509_pop_free(pChain, X509_free);
        return false;
    }

#ifdef SSL_CTX_set0_chain
    if (SSL_CTX_set0_chain(pSSLContext->pCtx, pChain) == 0) {
        Log(eLog_error, "SSL_CTX_set0_chain(%s) error", szCert);
        sk_X509_pop_free(pChain, X509_free);
        return false;
    }
#else
    {
        int32_t n = sk_X509_num(pChain);
        while (n--) {
            pX509 = sk_X509_shift(pChain);

            if (SSL_CTX_add_extra_chain_cert(pSSLContext->pCtx, pX509) == 0) {
                Log(eLog_error, "SSL_CTX_add_extra_chain_cert(%s) error", szCert);
                sk_X509_pop_free(pChain, X509_free);
                return false;
            }
        }
        sk_X509_free(pChain);
    }
#endif

    EVP_PKEY* pkey = ssl_load_certificate_key(szKey, nKeyLength, szPassword);
    if (pkey == NULL) {
        return false;
    }

    if (SSL_CTX_use_PrivateKey(pSSLContext->pCtx, pkey) == 0) {
        Log(eLog_error, "SSL_CTX_use_PrivateKey(%s) error", szKey);
        EVP_PKEY_free(pkey);
        return false;
    }
    EVP_PKEY_free(pkey);
    return true;
}

#if (OPENSSL_VERSION_NUMBER < 0x10100001L && !defined LIBRESSL_VERSION_NUMBER)

static RSA* ssl_rsa512_key_callback(sslConnection_tt* pSSLConnection, int32_t iIsExport,
                                    int32_t iKeyLength)
{
    static RSA* pKey;
    if (iKeyLength != 512) {
        return NULL;
    }

#    if (OPENSSL_VERSION_NUMBER < 0x10100003L && !defined OPENSSL_NO_DEPRECATED)

    if (pKey == NULL) {
        pKey = RSA_generate_key(512, RSA_F4, NULL, NULL);
    }
#    endif
    return pKey;
}

#endif

bool sslContext_ciphers(sslContext_tt* pSSLContext, const char* szCiphers,
                        bool bPreferServerCiphers)
{
    if (SSL_CTX_set_cipher_list(pSSLContext->pCtx, szCiphers) == 0) {
        return false;
    }

    if (bPreferServerCiphers) {
        SSL_CTX_set_options(pSSLContext->pCtx, SSL_OP_CIPHER_SERVER_PREFERENCE);
    }

#if (OPENSSL_VERSION_NUMBER < 0x10100001L && !defined LIBRESSL_VERSION_NUMBER)
    /* a temporary 512-bit RSA key is required for export versions of MSIE */
    SSL_CTX_set_tmp_rsa_callback(pSSLContext->pCtx, ssl_rsa512_key_callback);
#endif

    return true;
}

bool sslContext_clientCertificate(sslContext_tt* pSSLContext, const char* szCert, int32_t iDepth)
{
    SSL_CTX_set_verify(pSSLContext->pCtx, SSL_VERIFY_PEER, NULL);

    SSL_CTX_set_verify_depth(pSSLContext->pCtx, iDepth);

    if (szCert == NULL) {
        return true;
    }

    if (SSL_CTX_load_verify_locations(pSSLContext->pCtx, szCert, NULL) == 0) {
        return false;
    }

    ERR_clear_error();

    STACK_OF(X509_NAME)* pList = SSL_load_client_CA_file(szCert);

    if (pList == NULL) {
        return false;
    }

    SSL_CTX_set_client_CA_list(pSSLContext->pCtx, pList);

    return true;
}

bool sslContext_trustedCertificate(sslContext_tt* pSSLContext, const char* szCert, int32_t iDepth)
{
    SSL_CTX_set_verify_depth(pSSLContext->pCtx, iDepth);

    if (szCert == NULL) {
        return true;
    }

    if (SSL_CTX_load_verify_locations(pSSLContext->pCtx, szCert, NULL) == 0) {
        return false;
    }

    ERR_clear_error();

    return true;
}

bool sslContext_crl(sslContext_tt* pSSLContext, const char* szCrl)
{
    if (szCrl == NULL) {
        return true;
    }

    X509_STORE* pStore = SSL_CTX_get_cert_store(pSSLContext->pCtx);

    if (pStore == NULL) {
        return false;
    }

    X509_LOOKUP* pLookup = X509_STORE_add_lookup(pStore, X509_LOOKUP_file());

    if (pLookup == NULL) {
        return false;
    }

    if (X509_LOOKUP_load_file(pLookup, szCrl, X509_FILETYPE_PEM) == 0) {
        return false;
    }

    X509_STORE_set_flags(pStore, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    return true;
}

struct sslConnection_s
{
    sslContext_tt* pSSLContext;
    SSL*           pSSL;
    BIO*           pInBIO;
    BIO*           pOutBIO;
    atomic_int     iRefCount;
};

sslConnection_tt* createSSLConnection(sslContext_tt* pSSLContext, bool bAccept)
{
    sslConnection_tt* pConnection = mem_malloc(sizeof(sslConnection_tt));
    if (pConnection == NULL) {
        return false;
    }
    // #ifdef SSL_READ_EARLY_DATA_SUCCESS
    //     if (SSL_CTX_get_max_early_data(pSSL->pCtx))
    //     {
    //         pConnection->try_early_data = 1;//
    //     }
    // #endif
    pConnection->pSSL = SSL_new(pSSLContext->pCtx);

    if (pConnection->pSSL == NULL) {
        return false;
    }

    pConnection->pInBIO = BIO_new(BIO_s_mem());
    if (pConnection->pInBIO == NULL) {
        return false;
    }
    BIO_set_mem_eof_return(pConnection->pInBIO, -1);

    pConnection->pOutBIO = BIO_new(BIO_s_mem());
    if (pConnection->pOutBIO == NULL) {
        return false;
    }
    BIO_set_mem_eof_return(pConnection->pOutBIO, -1);

    SSL_set_bio(pConnection->pSSL, pConnection->pInBIO, pConnection->pOutBIO);

    if (bAccept) {
        SSL_set_accept_state(pConnection->pSSL);
#ifdef SSL_OP_NO_RENEGOTIATION
        SSL_set_options(pConnection->pSSL, SSL_OP_NO_RENEGOTIATION);
#endif
    }
    else {
        SSL_set_connect_state(pConnection->pSSL);
    }

    if (SSL_set_ex_data(pConnection->pSSL, s_iConnectionIndex, pConnection) == 0) {
        return false;
    }
    pConnection->pSSLContext = pSSLContext;
    atomic_init(&pConnection->iRefCount, 1);
    return pConnection;
}

static int32_t sslConnection_bioWrite(sslConnection_tt* pSSLConnection, const char* pBuffer,
                                      size_t nLength)
{
    const char* p = pBuffer;
    size_t      n = nLength;
    while (n > 0) {
        int32_t iWritten = BIO_write(pSSLConnection->pInBIO, p, n);
        if (iWritten <= 0) {
            return -1;
        }
        else if (iWritten <= n) {
            p += iWritten;
            n -= iWritten;
        }
        else {
            return -1;
        }
    }
    return nLength;
}

static bool sslConnection_bioRead(sslConnection_tt* pSSLConnection, byteQueue_tt* pByteQueue)
{
    char    pBuffer[4096];
    int32_t iPending = BIO_ctrl_pending(pSSLConnection->pOutBIO);
    if (iPending > 0) {
        do {
            int32_t iRead = BIO_read(pSSLConnection->pOutBIO, pBuffer, 4096);
            if (iRead <= 0) {
                return false;
            }
            else if (iRead <= 4096) {
                byteQueue_write(pByteQueue, pBuffer, iRead);
            }
            else {
                return false;
            }
            iPending = BIO_ctrl_pending(pSSLConnection->pOutBIO);
        } while (iPending > 0);
    }
    return true;
}

int32_t sslConnection_handshake(sslConnection_tt* pSSLConnection, const char* pBuffer,
                                size_t nLength, byteQueue_tt* pByteQueue)
{
    if (SSL_is_init_finished(pSSLConnection->pSSL)) {
        return 1;
    }

    if (nLength > 0 && pBuffer != NULL) {
        if (sslConnection_bioWrite(pSSLConnection, pBuffer, nLength) != nLength) {
            return -1;
        }
    }

    if (!SSL_is_init_finished(pSSLConnection->pSSL)) {
        int32_t iRet = SSL_do_handshake(pSSLConnection->pSSL);
        if (iRet == 1) {
            return 0;
        }
        else if (iRet < 0) {
            int32_t iErr = SSL_get_error(pSSLConnection->pSSL, iRet);
            if (iErr == SSL_ERROR_WANT_READ || iErr == SSL_ERROR_WANT_WRITE) {
                if (!sslConnection_bioRead(pSSLConnection, pByteQueue)) {
                    return -1;
                }
                return 1;
            }
            else {
                return -1;
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}

void sslConnection_addref(sslConnection_tt* pSSLConnection)
{
    atomic_fetch_add(&(pSSLConnection->iRefCount), 1);
}

void sslConnection_release(sslConnection_tt* pSSLConnection)
{
    if (atomic_fetch_sub(&(pSSLConnection->iRefCount), 1) == 1) {
        if (pSSLConnection->pSSL) {
            SSL_free(pSSLConnection->pSSL);
        }
        mem_free(pSSLConnection);
    }
}

bool sslConnection_read(sslConnection_tt* pSSLConnection, const char* pBuffer, size_t nLength,
                        byteQueue_tt* pByteQueue)
{
    if (nLength > 0 && pBuffer) {
        if (sslConnection_bioWrite(pSSLConnection, pBuffer, nLength) != nLength) {
            return false;
        }
    }

    char Buffer[4096];
    int  read = 0;

    do {
        int32_t iRead = SSL_read(pSSLConnection->pSSL, Buffer, 4096);
        if (read <= 0) {
            int32_t iErr = SSL_get_error(pSSLConnection->pSSL, iRead);
            if (iErr == SSL_ERROR_WANT_READ || iErr == SSL_ERROR_WANT_WRITE) {
                break;
            }
            return false;
        }
        else if (read <= 4096) {
            byteQueue_write(pByteQueue, Buffer, iRead);
        }
        else {
            return false;
        }
    } while (true);

    return true;
}

bool sslConnection_write(sslConnection_tt* pSSLConnection, const char* pBuffer, size_t nLength,
                         byteQueue_tt* pByteQueue)
{
    const char* pWriteBuffer = pBuffer;
    size_t      nWriteLength = nLength;
    while (nWriteLength > 0) {
        int32_t iWritten = SSL_write(pSSLConnection->pSSL, pWriteBuffer, nWriteLength);
        if (iWritten <= 0) {
            return false;
        }
        else if (iWritten <= nWriteLength) {
            pWriteBuffer += iWritten;
            nWriteLength -= iWritten;
        }
        else {
            return false;
        }
    }

    return sslConnection_bioRead(pSSLConnection, pByteQueue);
}
