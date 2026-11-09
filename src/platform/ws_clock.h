/* Worldseed Engine — monotonic clock adapter.
 *
 * Wall-clock time never drives simulation; it is used only for profiling,
 * frame pacing and logging. Simulation advances by fixed steps, not by real
 * elapsed time (see docs/03-C17-MIMARI.md, time model).
 */
#ifndef WS_PLATFORM_CLOCK_H
#define WS_PLATFORM_CLOCK_H

#include <stdint.h>

/* Monotonic nanoseconds since an unspecified origin. Never decreases. */
uint64_t ws_clock_monotonic_ns(void);

/* Convert a nanosecond duration to seconds. */
double ws_clock_ns_to_seconds(uint64_t ns);

#endif /* WS_PLATFORM_CLOCK_H */
