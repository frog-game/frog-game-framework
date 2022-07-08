#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "eventIO/eventIO_t.h"

struct eventIOThread_s;

typedef struct eventIOThread_s eventIOThread_tt;

frCore_API eventIOThread_tt* createEventIOThread(eventIO_tt* pEventIO);

frCore_API void eventIOThread_addref(eventIOThread_tt* pEventIOThread);

frCore_API void eventIOThread_release(eventIOThread_tt* pEventIOThread);

frCore_API void eventIOThread_start(eventIOThread_tt* pEventIOThread, bool bWaitThreadStarted,
                                    bool (*fnPre)(eventIOThread_tt*),
                                    void (*fnPost)(eventIOThread_tt*));

frCore_API void eventIOThread_stop(eventIOThread_tt* pEventIOThread, bool bWaitThreadStarted);

frCore_API void eventIOThread_join(eventIOThread_tt* pEventIOThread);

frCore_API bool eventIOThread_isRunning(eventIOThread_tt* pEventIOThread);

frCore_API bool eventIOThread_isStopped(eventIOThread_tt* pEventIOThread);

frCore_API bool eventIOThread_isStopping(eventIOThread_tt* pEventIOThread);

frCore_API eventIO_tt* eventIOThread_getEventIO(eventIOThread_tt* pEventIOThread);
