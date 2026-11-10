/* Worldseed Engine — hardware discovery (P10).
 *
 * Reports the machine profile the performance policy needs (docs/11): CPU
 * thread count, physical RAM, OS, and the OpenCL device summary. No number here
 * is a declared minimum requirement; the profile only informs adaptive quality
 * and benchmark labelling.
 */
#ifndef WS_PLATFORM_HARDWARE_H
#define WS_PLATFORM_HARDWARE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_opencl_probe.h"

typedef struct ws_hardware_info {
    uint32_t cpu_threads;      /* logical hardware threads; 0 when unknown */
    uint64_t ram_bytes;        /* physical memory; 0 when unknown */
    char os_name[32];          /* "Windows", "Linux", "macOS", "unknown" */
    ws_opencl_info opencl;     /* device discovery result (may be absent) */
} ws_hardware_info;

/* Fill *out with the detected machine profile. Never fails on missing data;
 * unavailable fields are left zero/empty. */
ws_result ws_hardware_probe(ws_hardware_info *out);

/* Human-readable profile line for logs, e.g.
 * "cpu_threads=8 ram_gb=31.9 os=Linux opencl=1 device=..." */
void ws_hardware_describe(const ws_hardware_info *info, char *buf, size_t cap);

#endif /* WS_PLATFORM_HARDWARE_H */
