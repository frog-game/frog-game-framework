#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


#if defined(_WINDOWS) || defined(_WIN32)
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <sys/socket.h>

#endif

#include "hash_t.h"
#include "platform_t.h"


typedef struct inetAddress_s
{
    union
    {
        struct sockaddr_in  address;
        struct sockaddr_in6 address_inV6;
    };
} inetAddress_tt;

frCore_API bool inetAddress_init(inetAddress_tt* pInetAddress, const char* szAddr, uint16_t uiPort,
                                 bool bInV6);

frCore_API void inetAddress_init_any(inetAddress_tt* pInetAddress, uint16_t uiPort, bool bInV6);

frCore_API void inetAddress_init_V4(inetAddress_tt* pInetAddress, struct sockaddr_in address);

frCore_API void inetAddress_init_V6(inetAddress_tt* pInetAddress, struct sockaddr_in6 address);

frCore_API bool inetAddress_init_fromIpPort(inetAddress_tt* pInetAddress, const char* szBuffer);

frCore_API bool inetAddress_toIPPortString(inetAddress_tt* pInetAddress, char* szBuffer,
                                           size_t nLength);

frCore_API bool inetAddress_toIPString(inetAddress_tt* pInetAddresss, char* szBuffer,
                                       size_t nLength);

static inline uint16_t inetAddress_getPort(const inetAddress_tt* pInetAddresss)
{
    return ntohs(pInetAddresss->address.sin_port);
}

static inline uint32_t inetAddress_getNetworkIP(const inetAddress_tt* pInetAddresss)
{
    return pInetAddresss->address.sin_addr.s_addr;
}

static inline uint16_t inetAddress_getNetworkPort(const inetAddress_tt* pInetAddresss)
{
    return pInetAddresss->address.sin_port;
}

static inline bool inetAddress_isIpV4(const inetAddress_tt* pInetAddresss)
{
    return pInetAddresss->address.sin_family == AF_INET;
}

static inline bool inetAddress_isIpV6(const inetAddress_tt* pInetAddresss)
{
    return pInetAddresss->address.sin_family == AF_INET6;
}

static inline struct sockaddr* inetAddress_getSockaddr(const inetAddress_tt* pInetAddresss)
{
    return (struct sockaddr*)(&pInetAddresss->address);
}

static inline socklen_t inetAddress_getSocklen(const inetAddress_tt* pInetAddresss)
{
    if (pInetAddresss->address.sin_family == AF_INET) {
        return sizeof(struct sockaddr_in);
    }
    else {
        return sizeof(struct sockaddr_in6);
    }
}

static inline uint64_t inetAddress_hash(const inetAddress_tt* pInetAddresss)
{
    if (pInetAddresss->address.sin_family == AF_INET) {
        return fnv32_buf(
            &pInetAddresss->address, offsetof(struct sockaddr_in, sin_zero), FNV_32_HASH_START);
    }
    else {
        return fnv64_buf(
            &pInetAddresss->address_inV6, sizeof(struct sockaddr_in6), FNV_64_HASH_START);
    }
}

static inline int32_t inetAddressCmp(const inetAddress_tt* src, const inetAddress_tt* dst)
{
    if (src->address.sin_family == AF_INET6) {
        return memcmp(&src->address_inV6, &dst->address_inV6, sizeof(struct sockaddr_in6));
    }
    return memcmp(&src->address, &dst->address, offsetof(struct sockaddr_in, sin_zero));
}