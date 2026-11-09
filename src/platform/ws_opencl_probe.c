/* Worldseed Engine — OpenCL 1.2 capability probe (P10 feasibility prototype).
 *
 * Minimal local declarations of the OpenCL 1.2 C API subset used here. These
 * match the Khronos OpenCL 1.2 API; no OpenCL header or runtime is bundled.
 */
#include "ws_opencl_probe.h"

#include "ws_dynlib.h"

#include <stddef.h>
#include <string.h>

typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef uint64_t cl_ulong;
typedef void *cl_platform_id;
typedef void *cl_device_id;

#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_PLATFORM_NAME 0x0902
#define CL_PLATFORM_VERSION 0x0901
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_VERSION 0x102F
#define CL_DEVICE_TYPE 0x1000
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x101F
#define CL_DEVICE_TYPE_CPU (1 << 1)
#define CL_DEVICE_TYPE_GPU (1 << 2)

typedef cl_int (*clGetPlatformIDs_fn)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int (*clGetPlatformInfo_fn)(cl_platform_id, cl_uint, size_t, void *, size_t *);
typedef cl_int (*clGetDeviceIDs_fn)(cl_platform_id, cl_ulong, cl_uint, cl_device_id *, cl_uint *);
typedef cl_int (*clGetDeviceInfo_fn)(cl_device_id, cl_uint, size_t, void *, size_t *);

/* ISO C forbids casting an object pointer (dlsym/GetProcAddress result) to a
 * function pointer. Converting through a union of the two pointer kinds is the
 * standard-conformant, implementation-defined way to do it and keeps this
 * platform-only adapter free of -Wpedantic violations. Supported platforms
 * (Windows, POSIX) represent both pointer kinds identically. */
static clGetPlatformIDs_fn ws_symbol_cast_pf_platform_ids(void *sym)
{
    union { void *obj; clGetPlatformIDs_fn fn; } u;
    u.obj = sym;
    return u.fn;
}
static clGetPlatformInfo_fn ws_symbol_cast_pf_platform_info(void *sym)
{
    union { void *obj; clGetPlatformInfo_fn fn; } u;
    u.obj = sym;
    return u.fn;
}
static clGetDeviceIDs_fn ws_symbol_cast_pf_device_ids(void *sym)
{
    union { void *obj; clGetDeviceIDs_fn fn; } u;
    u.obj = sym;
    return u.fn;
}
static clGetDeviceInfo_fn ws_symbol_cast_pf_device_info(void *sym)
{
    union { void *obj; clGetDeviceInfo_fn fn; } u;
    u.obj = sym;
    return u.fn;
}

ws_result ws_opencl_probe(ws_opencl_info *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(out, 0, sizeof(*out));

    static const char *const candidates[] = {
#if defined(_WIN32)
        "OpenCL.dll",
        "opencl.dll"
#else
        "libOpenCL.so.1",
        "libOpenCL.so"
#endif
    };

    ws_dynlib *lib = NULL;
    if (ws_dynlib_open_any(candidates, sizeof(candidates) / sizeof(candidates[0]), &lib) != WS_OK) {
        return WS_OK; /* No ICD: CPU fallback. Not an error. */
    }
    out->runtime_loaded = 1;

    clGetPlatformIDs_fn get_platform_ids =
        ws_symbol_cast_pf_platform_ids(ws_dynlib_symbol(lib, "clGetPlatformIDs"));
    clGetPlatformInfo_fn get_platform_info =
        ws_symbol_cast_pf_platform_info(ws_dynlib_symbol(lib, "clGetPlatformInfo"));
    clGetDeviceIDs_fn get_device_ids =
        ws_symbol_cast_pf_device_ids(ws_dynlib_symbol(lib, "clGetDeviceIDs"));
    clGetDeviceInfo_fn get_device_info =
        ws_symbol_cast_pf_device_info(ws_dynlib_symbol(lib, "clGetDeviceInfo"));

    if (get_platform_ids == NULL || get_platform_info == NULL ||
        get_device_ids == NULL || get_device_info == NULL) {
        ws_dynlib_close(lib);
        return WS_OK;
    }

    cl_uint platform_count = 0;
    if (get_platform_ids(0u, NULL, &platform_count) != CL_SUCCESS || platform_count == 0u) {
        ws_dynlib_close(lib);
        return WS_OK;
    }
    out->platform_count = platform_count;

    cl_platform_id platforms[16];
    cl_uint got = platform_count > 16u ? 16u : platform_count;
    if (get_platform_ids(got, platforms, NULL) != CL_SUCCESS) {
        ws_dynlib_close(lib);
        return WS_OK;
    }

    for (cl_uint i = 0; i < got && !out->device_found; ++i) {
        if (out->platform_name[0] == '\0') {
            get_platform_info(platforms[i], CL_PLATFORM_NAME, sizeof(out->platform_name),
                              out->platform_name, NULL);
        }
        cl_device_id device = NULL;
        cl_uint device_count = 0;
        cl_int rc = get_device_ids(platforms[i], CL_DEVICE_TYPE_ALL, 1u, &device, &device_count);
        if (rc != CL_SUCCESS || device_count == 0u || device == NULL) {
            continue;
        }
        get_device_info(device, CL_DEVICE_NAME, sizeof(out->device_name), out->device_name, NULL);
        get_device_info(device, CL_DEVICE_VERSION, sizeof(out->device_version),
                        out->device_version, NULL);
        cl_ulong units = 0;
        get_device_info(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(units), &units, NULL);
        cl_ulong mem = 0;
        get_device_info(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem), &mem, NULL);
        out->compute_units = (uint32_t)units;
        out->global_memory_bytes = (uint64_t)mem;
        out->device_found = 1;
    }

    ws_dynlib_close(lib);
    return WS_OK;
}
