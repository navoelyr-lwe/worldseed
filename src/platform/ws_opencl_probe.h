/* Worldseed Engine — OpenCL 1.2 capability probe (P10 feasibility prototype).
 *
 * Loads the system OpenCL ICD at runtime and reports whether an OpenCL 1.2
 * compatible platform/device is present. It uses only the OpenCL 1.2 C API
 * subset and never ships an OpenCL runtime. When no device is found the engine
 * uses the CPU fallback; the world must still open.
 *
 * Only the minimum OpenCL declarations this probe needs are declared locally so
 * the shared core has no external header dependency.
 */
#ifndef WS_PLATFORM_OPENCL_PROBE_H
#define WS_PLATFORM_OPENCL_PROBE_H

#include <stdint.h>

#include "../core/ws_result.h"

typedef struct ws_opencl_info {
    int runtime_loaded;      /* 1 if the ICD loaded and clGetPlatformIDs resolved */
    int device_found;        /* 1 if a usable device was found */
    uint32_t platform_count;
    char device_name[128];
    char platform_name[128];
    char device_version[64];
    uint32_t compute_units;
    uint64_t global_memory_bytes;
} ws_opencl_info;

/* Probe the system for an OpenCL 1.2 compatible device.
 * Always returns WS_OK and fills *out; absence of a device is not an error. */
ws_result ws_opencl_probe(ws_opencl_info *out);

#endif /* WS_PLATFORM_OPENCL_PROBE_H */
