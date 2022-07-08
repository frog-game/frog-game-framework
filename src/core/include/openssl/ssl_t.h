#pragma once

// type
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "byteQueue_t.h"

#define DEF_SSL_BUFFER_SIZE 16384

#define DEF_SSL_SSLv2 0x0001
#define DEF_SSL_SSLv3 0x0002
#define DEF_SSL_TLSv1 0x0004
#define DEF_SSL_TLSv1_1 0x0008
#define DEF_SSL_TLSv1_2 0x0010
#define DEF_SSL_TLSv1_3 0x0020

frCore_API bool sslInit();

frCore_API void sslExit();

struct sslContext_s;
struct sslConnection_s;

typedef struct sslContext_s    sslContext_tt;
typedef struct sslConnection_s sslConnection_tt;

frCore_API sslContext_tt* createSSLContext(uint32_t uiProtocols, void* pData);

frCore_API void sslContext_addref(sslContext_tt* pSSLContext);

frCore_API void sslContext_release(sslContext_tt* pSSLContext);

frCore_API bool sslContext_loadCertificate(sslContext_tt* pSSLContext, const char* szCert,
                                           size_t nCertLength, char* szKey, size_t nKeyLength,
                                           const char* szPassword);

frCore_API bool sslContext_ciphers(sslContext_tt* pSSLContext, const char* szCiphers,
                                   bool bPreferServerCiphers);

frCore_API bool sslContext_clientCertificate(sslContext_tt* pSSLContext, const char* szCert,
                                             int32_t iDepth);

frCore_API bool sslContext_trustedCertificate(sslContext_tt* pSSLContext, const char* szCert,
                                              int32_t iDepth);

frCore_API bool sslContext_crl(sslContext_tt* pSSLContext, const char* szCrl);

frCore_API sslConnection_tt* createSSLConnection(sslContext_tt* pSSLContext, bool bAccept);

frCore_API void sslConnection_addref(sslConnection_tt* pSSLConnection);

frCore_API void sslConnection_release(sslConnection_tt* pSSLConnection);

frCore_API int32_t sslConnection_handshake(sslConnection_tt* pSSLConnection, const char* pBuffer,
                                           size_t nLength, byteQueue_tt* pByteQueue);

frCore_API bool sslConnection_read(sslConnection_tt* pSSLConnection, const char* pBuffer,
                                   size_t nLength, byteQueue_tt* pByteQueue);

frCore_API bool sslConnection_write(sslConnection_tt* pSSLConnection, const char* pBuffer,
                                    size_t nLength, byteQueue_tt* pByteQueue);
