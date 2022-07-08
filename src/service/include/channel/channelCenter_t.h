

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "service_t.h"

struct channel_s;

frService_API void channelCenter_init();

frService_API void channelCenter_clear();

frService_API uint32_t channelCenter_register(struct channel_s* pHandle);

frService_API bool channelCenter_deregister(uint32_t uiChannelID);

frService_API struct channel_s* channelCenter_gain(uint32_t uiChannelID);

frService_API uint32_t* channelCenter_getIds(int32_t* pCount);
