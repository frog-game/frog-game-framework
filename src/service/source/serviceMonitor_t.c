

#include "serviceMonitor_t.h"

#include "eventIO/eventIO_t.h"

#include "thread_t.h"
#include "utility_t.h"

#include "internal/service-inl.h"
#include "serviceCenter_t.h"
#include "serviceEvent_t.h"
#include "serviceMonitor_t.h"
#include "service_t.h"

#include <stdatomic.h>
#include <stdlib.h>

typedef struct monitor_s
{
    atomic_int iVersion;
    int32_t    iCheckVersion;
    uint32_t   uiSourceID;
    uint32_t   uiDestinationID;
} monitor_tt;

typedef struct serviceMonitor_s
{
    uint32_t       uiMonitorID;
    monitor_tt*    pMonitorSlot;
    uint32_t       uiMonitorSlotCount;
    eventTimer_tt* pEventTimer;
    atomic_bool    bRunning;
    atomic_int     iIndex;
} serviceMonitor_tt;

static serviceMonitor_tt* s_pServiceMonitor = NULL;

static void serviceMonitor_check(eventTimer_tt* pEventTimer, void* pData)
{
    serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;
    if (pServiceMonitor) {
        if (atomic_load(&(pServiceMonitor->bRunning))) {
            int32_t iVersion = 0;
            for (uint32_t i = 0; i < pServiceMonitor->uiMonitorSlotCount; ++i) {
                iVersion = atomic_load(&(pServiceMonitor->pMonitorSlot[i].iVersion));
                if (iVersion == pServiceMonitor->pMonitorSlot[i].iCheckVersion) {
                    if (pServiceMonitor->pMonitorSlot[i].uiDestinationID != 0) {
                        service_tt* pServiceHandle =
                            serviceCenter_gain(pServiceMonitor->uiMonitorID);
                        if (pServiceHandle) {
                            service_send(pServiceHandle,
                                         pServiceMonitor->pMonitorSlot[i].uiDestinationID,
                                         NULL,
                                         0,
                                         DEF_EVENT_MSG | DEF_EVENT_MSG_TEXT,
                                         0);
                            service_release(pServiceHandle);
                        }
                        pServiceMonitor->pMonitorSlot[i].uiDestinationID = 0;
                    }
                }
                else {
                    pServiceMonitor->pMonitorSlot[i].iCheckVersion = iVersion;
                }
            }
        }
    }
}

void serviceMonitor_init(uint32_t uiNumberOfConcurrentThreads)
{
    if (s_pServiceMonitor == NULL) {
        serviceMonitor_tt* pServiceMonitor = mem_malloc(sizeof(serviceMonitor_tt));
        pServiceMonitor->uiMonitorID       = 0;
        if (uiNumberOfConcurrentThreads <= 0) {
            pServiceMonitor->pMonitorSlot       = mem_malloc(sizeof(monitor_tt));
            pServiceMonitor->uiMonitorSlotCount = 1;
        }
        else {
            pServiceMonitor->pMonitorSlot =
                mem_malloc(sizeof(monitor_tt) * uiNumberOfConcurrentThreads);
            pServiceMonitor->uiMonitorSlotCount = uiNumberOfConcurrentThreads;
        }
        for (uint32_t i = 0; i < pServiceMonitor->uiMonitorSlotCount; ++i) {
            atomic_init(&(pServiceMonitor->pMonitorSlot[i].iVersion), 0);
            pServiceMonitor->pMonitorSlot[i].iCheckVersion   = 0;
            pServiceMonitor->pMonitorSlot[i].uiSourceID      = 0;
            pServiceMonitor->pMonitorSlot[i].uiDestinationID = 0;
        }
        atomic_init(&pServiceMonitor->iIndex, 0);
        atomic_init(&pServiceMonitor->bRunning, false);
        s_pServiceMonitor = pServiceMonitor;
    }
}

bool serviceMonitor_start(uint32_t uiMonitorID, struct eventIO_s* pEventIO, uint32_t uiIntervalMs)
{
    serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;

    if ((pServiceMonitor != NULL) && !atomic_load(&(pServiceMonitor->bRunning))) {
        pServiceMonitor->uiMonitorID = uiMonitorID;
        pServiceMonitor->pEventTimer =
            createEventTimer(pEventIO, serviceMonitor_check, false, uiIntervalMs, NULL);
        eventTimer_start(s_pServiceMonitor->pEventTimer);
        atomic_store(&pServiceMonitor->bRunning, true);
        return true;
    }
    return false;
}

void serviceMonitor_stop()
{
    serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;
    if (pServiceMonitor == NULL) {
        return;
    }

    bool bRunning = true;
    if (atomic_compare_exchange_strong(&pServiceMonitor->bRunning, &bRunning, false)) {
        if (pServiceMonitor->pEventTimer) {
            eventTimer_stop(pServiceMonitor->pEventTimer);
            eventTimer_release(pServiceMonitor->pEventTimer);
            pServiceMonitor->pEventTimer = NULL;
        }
    }
}

void serviceMonitor_clear()
{
    if (s_pServiceMonitor != NULL) {
        serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;
        s_pServiceMonitor                  = NULL;
        if (pServiceMonitor->pMonitorSlot) {
            mem_free(pServiceMonitor->pMonitorSlot);
            pServiceMonitor->pMonitorSlot = NULL;
        }
        mem_free(pServiceMonitor);
    }
}

int32_t serviceMonitor_enter(uint32_t uiSourceID, uint32_t uiDestinationID)
{
    serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;
    if (pServiceMonitor == NULL) {
        return -1;
    }

    static _decl_threadLocal int32_t s_iThreadIndex = -1;
    if (s_iThreadIndex == -1) {
        s_iThreadIndex = atomic_fetch_add(&(pServiceMonitor->iIndex), 1);
    }

    pServiceMonitor->pMonitorSlot[s_iThreadIndex].uiSourceID      = uiSourceID;
    pServiceMonitor->pMonitorSlot[s_iThreadIndex].uiDestinationID = uiDestinationID;
    atomic_fetch_add(&(pServiceMonitor->pMonitorSlot[s_iThreadIndex].iVersion), 1);
    return s_iThreadIndex;
}

void serviceMonitor_leave(int32_t iIndex)
{
    serviceMonitor_tt* pServiceMonitor = s_pServiceMonitor;
    if (pServiceMonitor == NULL) {
        return;
    }

    pServiceMonitor->pMonitorSlot[iIndex].uiSourceID      = 0;
    pServiceMonitor->pMonitorSlot[iIndex].uiDestinationID = 0;
    atomic_fetch_add(&(pServiceMonitor->pMonitorSlot[iIndex].iVersion), 1);
}

int32_t serviceMonitor_waitForCount()
{
    return service_waitForCount();
}
