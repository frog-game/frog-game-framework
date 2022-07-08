#include "time_t.h"

#include <assert.h>

#if defined(_WINDOWS) || defined(_WIN32)
#    include <windows.h>
#elif defined(__APPLE__)
#    include <mach/mach.h>
#    include <mach/mach_time.h>
#    include <sys/time.h>
#else
#    include <time.h>
#endif

// #if defined( _WINDOWS )|| defined( _WIN32 )
// static inline double getNanosecsPerTic()
// {
// 	LARGE_INTEGER freq;
// 	if ( !QueryPerformanceFrequency( &freq ) )
// 		return 0.0L;
// 	return (double)(1000000000.0L / freq.QuadPart);
// }
// #endif

void getClockRealtime(timespec_tt* pTs)
{
#if defined(_WINDOWS) || defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    int64_t iClock =
        (((((int64_t)(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) - 116444736000000000LL) *
         100LL);
    pTs->iSec  = (long)(iClock / 1000000000L);
    pTs->iNsec = (long)(iClock % 1000000000L);
#elif defined(__APPLE__)
    struct timeval tv;
    gettimeofday(&tv, 0);
    pTs->iSec  = tv.tv_sec;
    pTs->iNsec = tv.tv_usec * 1000;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        assert(0 && "clock_gettime error");
        return;
    }
    pTs->iSec  = ts.tv_sec;
    pTs->iNsec = ts.tv_nsec;
#endif
}

void getClockMonotonic(timespec_tt* pTs)
{
#if defined(_WINDOWS) || defined(_WIN32)
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq)) {
        pTs->iSec  = 0;
        pTs->iNsec = 0;
        return;
    }

    if (freq.QuadPart <= 0) {
        pTs->iSec  = 0;
        pTs->iNsec = 0;
        return;
    }

    // double fNanosecsPerTic = getNanosecsPerTic();

    LARGE_INTEGER pcount;
    // if ( fNanosecsPerTic <= 0.0L )
    // {
    // 	pTs->iSec = 0;
    // 	pTs->iNsec = 0;
    // 	assert(0 && "getNanosecsPerTic error");
    // 	return;
    // }
    int32_t iTimer = 0;
    while (!QueryPerformanceCounter(&pcount)) {
        if (++iTimer > 3) {
            pTs->iSec  = 0;
            pTs->iNsec = 0;
            // assert(0 && "QueryPerformanceCounter error");
            return;
        }
    }
    // int64_t iClock = (int64_t)(fNanosecsPerTic * pcount.QuadPart);
    // pTs->iSec = iClock/1000000000L;
    // pTs->iNsec = iClock%1000000000L;

    long double ns     = 1000000000.0L * pcount.QuadPart / freq.QuadPart;
    int64_t     iClock = (int64_t)ns;
    pTs->iSec          = iClock / 1000000000L;
    pTs->iNsec         = iClock % 1000000000L;
#elif defined(__APPLE__)
    uint64_t                  uiClock;
    mach_timebase_info_data_t MachInfo;
    if (mach_timebase_info(&MachInfo) != KERN_SUCCESS) {
        assert(0 && "mach_timebase_info error");
        return;
    }

    if (MachInfo.numer == MachInfo.denom) {
        uiClock = mach_absolute_time();
    }
    else {
        uiClock = mach_absolute_time() * ((double)(MachInfo.numer) / MachInfo.denom);
    }
    pTs->iSec  = uiClock / 1000000000L;
    pTs->iNsec = uiClock % 1000000000L;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        assert(0 && "clock_gettime error");
        return;
    }
    pTs->iSec  = ts.tv_sec;
    pTs->iNsec = ts.tv_nsec;
#endif
}
