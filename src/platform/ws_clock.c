/* Worldseed Engine — monotonic clock adapter. */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
/* Strict C17 does not declare clock_gettime; request the POSIX.1-2001 clock
 * interface before any system header is pulled in. This is the platform
 * boundary. */
#define _POSIX_C_SOURCE 200112L
#endif

#include "ws_clock.h"

#if defined(_WIN32)
#include <windows.h>

uint64_t ws_clock_monotonic_ns(void)
{
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
        return 0u;
    }
    QueryPerformanceCounter(&counter);
    /* (counter * 1e9) / freq using integer math; split to avoid overflow. */
    uint64_t seconds = (uint64_t)(counter.QuadPart / freq.QuadPart);
    uint64_t remainder = (uint64_t)(counter.QuadPart % freq.QuadPart);
    return seconds * 1000000000ULL + (remainder * 1000000000ULL) / (uint64_t)freq.QuadPart;
}
#else
#include <time.h>

uint64_t ws_clock_monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0u;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

double ws_clock_ns_to_seconds(uint64_t ns)
{
    return (double)ns / 1000000000.0;
}
