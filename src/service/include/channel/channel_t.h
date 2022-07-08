

#pragma once

#include "byteQueue_t.h"
#include "inetAddress_t.h"
#include "service_t.h"

struct codecStream_s;

struct eventIO_s;
struct eventConnection_s;

struct channel_s;
typedef struct channel_s channel_tt;

frService_API channel_tt* createChannel(struct eventIO_s*         pEventIO,
                                        struct eventConnection_s* pEventConnection);

frService_API void channel_close(channel_tt* pHandle, int32_t iDisconnectTimeoutMs);

frService_API void channel_setCodecStream(channel_tt* pHandle, struct codecStream_s* pCodecStream);

frService_API void channel_addref(channel_tt* pHandle);

frService_API void channel_release(channel_tt* pHandle);

frService_API bool channel_bind(channel_tt* pHandle, service_tt* pService, bool bKeepAlive,
                                bool bTcpNoDelay);

frService_API bool channel_isRunning(channel_tt* pHandle);

frService_API bool channel_getRemoteAddr(channel_tt* pHandle, inetAddress_tt* pOutInetAddress);

frService_API bool channel_getLocalAddr(channel_tt* pHandle, inetAddress_tt* pOutInetAddress);

frService_API int32_t channel_send(channel_tt* pHandle, const char* pBuffer, int32_t iLength,
                                   uint32_t uiFlag, uint32_t uiToken);

frService_API int32_t channel_sendMove(channel_tt* pHandle, ioBufVec_tt* pInBufVec, int32_t iCount,
                                       uint32_t uiFlag, uint32_t uiToken);

frService_API int32_t channel_write(channel_tt* pHandle, const char* pBuffer, int32_t iLength,
                                    uint32_t uiToken);

frService_API int32_t channel_writeMove(channel_tt* pHandle, ioBufVec_tt* pInBufVec, int32_t iCount,
                                        uint32_t uiToken);

frService_API bool channel_pushService(channel_tt* pHandle, byteQueue_tt* pByteQueue,
                                       uint32_t uiLength, uint32_t uiFlag, uint32_t uiToken);

frService_API uint32_t channel_getID(channel_tt* pHandle);

frService_API service_tt* channel_getService(channel_tt* pHandle);

frService_API int32_t channel_getWritePending(channel_tt* pHandle);

frService_API size_t channel_getWritePendingBytes(channel_tt* pHandle);

frService_API size_t channel_getReceiveBufLength(channel_tt* pHandle);
