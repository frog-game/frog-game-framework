

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "service_t.h"

struct eventIO_s;

frService_API void serviceMonitor_init(uint32_t uiNumberOfConcurrentThreads);

frService_API void serviceMonitor_clear();

frService_API bool serviceMonitor_start(uint32_t uiMonitorID, struct eventIO_s* pEventIO,
                                        uint32_t uiIntervalMs);

frService_API void serviceMonitor_stop();

frService_API int32_t serviceMonitor_enter(uint32_t uiSourceID, uint32_t uiDestinationID);

frService_API void serviceMonitor_leave(int32_t iIndex);

frService_API int32_t serviceMonitor_waitForCount();
