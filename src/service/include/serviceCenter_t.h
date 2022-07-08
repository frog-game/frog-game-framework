

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "service_t.h"

frService_API void serviceCenter_init(int32_t iServerNodeId);

frService_API void serviceCenter_clear();

frService_API uint32_t serviceCenter_register(struct service_s* pHandle);

frService_API bool serviceCenter_deregister(uint32_t uiServiceID);

frService_API bool serviceCenter_bindName(uint32_t uiServiceID, const char* szName);

frService_API bool serviceCenter_unbindName(uint32_t uiServiceID);

frService_API uint32_t serviceCenter_findServiceID(const char* szName);

frService_API service_tt* serviceCenter_gain(uint32_t uiServiceID);
