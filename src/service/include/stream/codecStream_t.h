

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eventIO/eventIO_t.h"
#include "service_t.h"

struct channel_s;

typedef struct codecStream_s
{
    int32_t (*fnWrite)(struct codecStream_s*, eventConnection_tt*, const char*, int32_t, uint32_t,
                       uint32_t);
    int32_t (*fnWriteMove)(struct codecStream_s*, eventConnection_tt*, ioBufVec_tt*, int32_t,
                           uint32_t, uint32_t);
    bool (*fnReceive)(struct codecStream_s*, struct channel_s*, byteQueue_tt*);
    void (*fnAddref)(struct codecStream_s*);
    void (*fnRelease)(struct codecStream_s*);
} codecStream_tt;
