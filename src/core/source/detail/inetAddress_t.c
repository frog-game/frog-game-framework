#include "inetAddress_t.h"

#include <stdio.h>
#include <stdlib.h>

#include "utility_t.h"

bool inetAddress_init(inetAddress_tt* pInetAddress, const char* szAddr, uint16_t uiPort, bool bInV6)
{
    if (!bInV6) {
        bzero(&(pInetAddress->address), sizeof(struct sockaddr_in));
        pInetAddress->address.sin_family = AF_INET;
        pInetAddress->address.sin_port   = htons(uiPort);
#if defined(__APPLE__)
        pInetAddress->address.sin_len = sizeof(struct sockaddr_in);
#endif
        if (inet_pton(AF_INET, szAddr, &(pInetAddress->address.sin_addr)) <= 0) {
            return false;
        }
    }
    else {
        bzero(&(pInetAddress->address_inV6), sizeof(struct sockaddr_in6));
        pInetAddress->address_inV6.sin6_family = AF_INET6;
        pInetAddress->address_inV6.sin6_port   = htons(uiPort);
#if defined(__APPLE__)
        pInetAddress->address_inV6.sin6_len = sizeof(struct sockaddr_in6);
#endif
        if (inet_pton(AF_INET6, szAddr, &(pInetAddress->address_inV6.sin6_addr)) <= 0) {
            return false;
        }
    }

    return true;
}

void inetAddress_init_any(inetAddress_tt* pInetAddress, uint16_t uiPort, bool bInV6)
{
    if (!bInV6) {
        bzero(&(pInetAddress->address), sizeof(struct sockaddr_in));
        pInetAddress->address.sin_family      = AF_INET;
        pInetAddress->address.sin_addr.s_addr = htonl(INADDR_ANY);
        pInetAddress->address.sin_port        = htons(uiPort);
#if defined(__APPLE__)
        pInetAddress->address.sin_len = sizeof(struct sockaddr_in);
#endif
    }
    else {
        bzero(&(pInetAddress->address_inV6), sizeof(struct sockaddr_in6));
        pInetAddress->address_inV6.sin6_family = AF_INET6;
        pInetAddress->address_inV6.sin6_addr   = in6addr_any;
        pInetAddress->address_inV6.sin6_port   = htons(uiPort);
#if defined(__APPLE__)
        pInetAddress->address_inV6.sin6_len = sizeof(struct sockaddr_in6);
#endif
    }
}

void inetAddress_init_V4(inetAddress_tt* pInetAddress, struct sockaddr_in address)
{
    pInetAddress->address = address;
}

void inetAddress_init_V6(inetAddress_tt* pInetAddress, struct sockaddr_in6 address)
{
    pInetAddress->address_inV6 = address;
}

bool inetAddress_init_fromIpPort(inetAddress_tt* pInetAddress, const char* szBuffer)
{
    const char* szFind = strrchr(szBuffer, ':');
    if (szFind == NULL || strlen(szFind + 1) == 0) {
        return false;
    }

    uint16_t uiPort = atoi(szFind + 1);

    size_t nLength = szFind - szBuffer;
    if (nLength == 0) {
        return false;
    }

    char* szHost = mem_malloc(nLength + 1);
    memcpy(szHost, szBuffer, nLength);
    szHost[nLength] = '\0';
    char* szAddr    = szHost;
    if (szHost[0] == '[' && szHost[nLength - 1] == ']') {
        szAddr              = szHost + 1;
        szAddr[nLength - 2] = '\0';
    }

    bool bIpV6 = false;
    if (strchr(szAddr, ':') != NULL) {
        bIpV6 = true;
    }
    inetAddress_init(pInetAddress, szAddr, uiPort, bIpV6);
    mem_free(szHost);
    return true;
}

bool inetAddress_toIPPortString(inetAddress_tt* pInetAddress, char* szBuffer, size_t nLength)
{
    if (pInetAddress->address.sin_family == AF_INET) {
        if (inet_ntop(AF_INET, &(pInetAddress->address.sin_addr), szBuffer, nLength)) {
            size_t nEnd = strlen(szBuffer);
            if (nLength - nEnd > 5) {
                uint16_t uiPort = ntohs(pInetAddress->address.sin_port);
                snprintf(szBuffer + nEnd, nLength - nEnd, ":%u", uiPort);
                return true;
            }
        }
    }
    else if (pInetAddress->address.sin_family == AF_INET6) {
        if (inet_ntop(AF_INET6, &(pInetAddress->address_inV6.sin6_addr), szBuffer, nLength)) {
            size_t nEnd = strlen(szBuffer);
            if (nLength - nEnd > 5) {
                uint16_t uiPort = ntohs(pInetAddress->address.sin_port);
                snprintf(szBuffer + nEnd, nLength - nEnd, ":%u", uiPort);
                return true;
            }
        }
    }
    return false;
}

bool inetAddress_toIPString(inetAddress_tt* pInetAddress, char* szBuffer, size_t nLength)
{
    if (pInetAddress->address.sin_family == AF_INET) {
        if (inet_ntop(AF_INET, &(pInetAddress->address.sin_addr), szBuffer, nLength)) {
            return true;
        }
    }
    else if (pInetAddress->address.sin_family == AF_INET6) {
        if (inet_ntop(AF_INET6, &(pInetAddress->address_inV6.sin6_addr), szBuffer, nLength)) {
            return true;
        }
    }
    return false;
}
