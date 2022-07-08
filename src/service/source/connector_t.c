

#include "internal/connector_t.h"

#include "internal/service-inl.h"
#include "serviceEvent_t.h"
#include "service_t.h"

static inline void connector_free(void* pData)
{
    connector_tt* pConnector = (connector_tt*)pData;
    connector_release(pConnector);
}

static inline void connector_onConnectingTimeout_close(eventTimer_tt* pHandle, void* pData)
{
    connector_tt* pConnector = (connector_tt*)pData;
    connector_release(pConnector);
}

static inline void connector_onConnector(eventConnection_tt* pHandle, void* pData)
{
    connector_tt* pConnector = (connector_tt*)pData;

    eventTimer_tt* pConnectingTimeoutHandle =
        (eventTimer_tt*)atomic_exchange(&pConnector->hConnectingTimeout, 0);
    if (pConnectingTimeoutHandle) {
        eventTimer_stop(pConnectingTimeoutHandle);
        eventTimer_release(pConnectingTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pConnector->hConnection, 0);

    if (pConnectionHandle) {
        if (!eventConnection_isConnecting(pConnectionHandle)) {
            eventConnection_release(pConnectionHandle);
            pConnectionHandle = NULL;
        }
        serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + sizeof(eventConnection_tt*));
        pEvent->uiSourceID      = service_getID(pConnector->pService);
        pEvent->uiToken         = pConnector->uiToken;
        *(eventConnection_tt**)(pEvent->szStorage) = pConnectionHandle;
        pEvent->uiLength                           = DEF_EVENT_CONNECT << 24;
        if (!service_enqueue(pConnector->pService, pEvent)) {
            if (pConnectionHandle) {
                eventConnection_release(pConnectionHandle);
                pConnectionHandle = NULL;
            }
        }
    }
}

static inline void connector_onConnectingTimeout(eventTimer_tt* pHandle, void* pData)
{
    connector_tt* pConnector = (connector_tt*)pData;

    eventTimer_tt* pConnectingTimeoutHandle =
        (eventTimer_tt*)atomic_exchange(&pConnector->hConnectingTimeout, 0);
    if (pConnectingTimeoutHandle) {
        eventTimer_release(pConnectingTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pConnector->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);

        serviceEvent_tt* pEvent = mem_malloc(sizeof(serviceEvent_tt) + sizeof(eventConnection_tt*));
        pEvent->uiSourceID      = service_getID(pConnector->pService);
        pEvent->uiToken         = pConnector->uiToken;
        *(eventConnection_tt**)(pEvent->szStorage) = NULL;
        pEvent->uiLength                           = DEF_EVENT_CONNECT << 24;
        service_enqueue(pConnector->pService, pEvent);
    }
}

connector_tt* createConnector(service_tt* pService, uint32_t uiToken)
{
    connector_tt* pHandle = mem_malloc(sizeof(connector_tt));
    pHandle->pService     = pService;
    service_addref(pHandle->pService);
    pHandle->uiToken = uiToken;
    atomic_init(&pHandle->hConnection, 0);
    atomic_init(&pHandle->hConnectingTimeout, 0);
    atomic_init(&pHandle->iRefCount, 1);
    return pHandle;
}

void connector_addref(connector_tt* pHandle)
{
    atomic_fetch_add(&(pHandle->iRefCount), 1);
}

void connector_release(connector_tt* pHandle)
{
    if (atomic_fetch_sub(&(pHandle->iRefCount), 1) == 1) {
        if (pHandle->pService) {
            service_release(pHandle->pService);
            pHandle->pService = NULL;
        }
        mem_free(pHandle);
    }
}

bool connector_connect(connector_tt* pHandle, const char* szAddress, int32_t iConnectingTimeoutMs,
                       bool bTcp)
{
    inetAddress_tt inetAddr;
    if (!inetAddress_init_fromIpPort(&inetAddr, szAddress)) {
        return false;
    }
    eventIO_tt* pEventIO = service_getEventIO(pHandle->pService);

    eventConnection_tt* pConnectionHandle = createEventConnection(pEventIO, &inetAddr, bTcp);
    atomic_store(&pHandle->hConnection, pConnectionHandle);
    eventConnection_setConnectorCallback(pConnectionHandle, connector_onConnector);

    if (iConnectingTimeoutMs > 0) {
        atomic_fetch_add(&(pHandle->iRefCount), 2);
        eventTimer_tt* pConnectingTimeoutHandle = createEventTimer(
            pEventIO, connector_onConnectingTimeout, true, iConnectingTimeoutMs, pHandle);
        eventTimer_setCloseCallback(pConnectingTimeoutHandle, connector_onConnectingTimeout_close);
        atomic_store(&pHandle->hConnectingTimeout, pConnectingTimeoutHandle);
        eventTimer_start(pConnectingTimeoutHandle);
    }
    else {
        atomic_fetch_add(&(pHandle->iRefCount), 1);
    }

    if (!eventConnection_connect(pConnectionHandle, pHandle, connector_free)) {
        pConnectionHandle = (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
        if (pConnectionHandle) {
            eventConnection_release(pConnectionHandle);
        }

        if (iConnectingTimeoutMs > 0) {
            eventTimer_tt* pConnectingTimeoutHandle =
                (eventTimer_tt*)atomic_exchange(&pHandle->hConnectingTimeout, 0);
            if (pConnectingTimeoutHandle) {
                eventTimer_stop(pConnectingTimeoutHandle);
                eventTimer_release(pConnectingTimeoutHandle);
            }
        }

        connector_release(pHandle);
        return false;
    }
    return true;
}

void connector_close(connector_tt* pHandle)
{
    eventTimer_tt* pConnectingTimeoutHandle =
        (eventTimer_tt*)atomic_exchange(&pHandle->hConnectingTimeout, 0);
    if (pConnectingTimeoutHandle) {
        eventTimer_stop(pConnectingTimeoutHandle);
        eventTimer_release(pConnectingTimeoutHandle);
    }

    eventConnection_tt* pConnectionHandle =
        (eventConnection_tt*)atomic_exchange(&pHandle->hConnection, 0);
    if (pConnectionHandle) {
        eventConnection_forceClose(pConnectionHandle);
        eventConnection_release(pConnectionHandle);
    }
}

service_tt* connector_getService(connector_tt* pHandle)
{
    return pHandle->pService;
}
