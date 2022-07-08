

#include "internal/listenPort_t.h"

#include "eventIO/eventIO_t.h"

#include "internal/service-inl.h"
#include "serviceEvent_t.h"
#include "service_t.h"

static inline void listenPort_onAccept(eventListenPort_tt* pHandle,
                                       eventConnection_tt* pEventConnection, const char* pBuffer,
                                       uint32_t uiLength, void* pData)
{
    listenPort_tt* plistenPort = (listenPort_tt*)pData;
    if (uiLength == 0) {
        serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + sizeof(eventConnection_tt*));
        pEvent->uiSourceID      = service_getID(plistenPort->pService);
        pEvent->uiToken         = 0;
        *(eventConnection_tt**)(pEvent->szStorage) = pEventConnection;
        pEvent->uiLength                           = DEF_EVENT_ACCEPT << 24;
        if (!service_enqueue(plistenPort->pService, pEvent)) {
            if (pEventConnection) {
                eventConnection_forceClose(pEventConnection);
                eventConnection_release(pEventConnection);
            }
        }
    }
    else {
        serviceEvent_tt* pEvent =
            mem_malloc(sizeof(serviceEvent_tt) + sizeof(eventConnection_tt*) + uiLength);
        pEvent->uiSourceID                         = service_getID(plistenPort->pService);
        pEvent->uiToken                            = 0;
        *(eventConnection_tt**)(pEvent->szStorage) = pEventConnection;
        memcpy(pEvent->szStorage + sizeof(eventConnection_tt*), pBuffer, uiLength);
        pEvent->uiLength = uiLength | (DEF_EVENT_ACCEPT << 24);
        if (!service_enqueue(plistenPort->pService, pEvent)) {
            if (pEventConnection) {
                eventConnection_forceClose(pEventConnection);
                eventConnection_release(pEventConnection);
            }
        }
    }
}

static inline void listenPort_onUserFree(void* pData)
{
    listenPort_tt* plistenPort = (listenPort_tt*)pData;
    listenPort_release(plistenPort);
}

listenPort_tt* createListenPort(service_tt* pService)
{
    listenPort_tt* pHandle = mem_malloc(sizeof(listenPort_tt));
    pHandle->pService      = pService;
    service_addref(pHandle->pService);
    pHandle->pListenPortHandle = NULL;
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void listenPort_addref(listenPort_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void listenPort_release(listenPort_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->pService) {
            service_release(pHandle->pService);
            pHandle->pService = NULL;
        }
        mem_free(pHandle);
    }
}

bool listenPort_start(listenPort_tt* pHandle, const char* szAddress, bool bTcp)
{
    inetAddress_tt inetAddr;
    if (!inetAddress_init_fromIpPort(&inetAddr, szAddress)) {
        return false;
    }
    eventIO_tt* pEventIO = service_getEventIO(pHandle->pService);
    atomic_fetch_add(&(pHandle->iRefCount), 1);

    pHandle->pListenPortHandle = createEventListenPort(pEventIO, &inetAddr, bTcp);
    eventListenPort_setAcceptCallback(pHandle->pListenPortHandle, listenPort_onAccept);
    return eventListenPort_start(pHandle->pListenPortHandle, pHandle, listenPort_onUserFree);
}

bool listenPort_postAccept(listenPort_tt* pHandle)
{
    if (pHandle->pListenPortHandle) {
        return eventListenPort_postAccept(pHandle->pListenPortHandle);
    }
    return false;
}

void listenPort_close(listenPort_tt* pHandle)
{
    if (pHandle->pListenPortHandle) {
        eventListenPort_close(pHandle->pListenPortHandle);
        eventListenPort_release(pHandle->pListenPortHandle);
        pHandle->pListenPortHandle = NULL;
    }
}

service_tt* listenPort_getService(listenPort_tt* pHandle)
{
    return pHandle->pService;
}
