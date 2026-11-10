/* Worldseed Engine — OpenCL 1.2 host wrapper (P10).
 *
 * Local declarations of the OpenCL 1.2 C API subset used here. They match the
 * Khronos OpenCL 1.2 API; no OpenCL header or runtime is bundled.
 */
#include "ws_opencl.h"

#include "../platform/ws_dynlib.h"

#include <stdlib.h>
#include <string.h>

/* The raster kernel source, embedded at configure time (src/CMakeLists.txt). */
extern const char ws_kernel_ws_raster[];

/* --- OpenCL 1.2 types and constants -------------------------------------- */
typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef uint64_t cl_ulong;
typedef uint64_t cl_bitfield;
typedef cl_bitfield cl_device_type;
typedef cl_bitfield cl_mem_flags;
typedef cl_bitfield cl_command_queue_properties;
typedef intptr_t cl_context_properties;
typedef void *cl_platform_id;
typedef void *cl_device_id;
typedef void *cl_context;
typedef void *cl_command_queue;
typedef void *cl_mem;
typedef void *cl_program;
typedef void *cl_kernel;

#define CL_SUCCESS 0
#define CL_DEVICE_NOT_FOUND (-1)
#define CL_DEVICE_NOT_AVAILABLE (-2)
#define CL_OUT_OF_RESOURCES (-5)
#define CL_OUT_OF_HOST_MEMORY (-6)
#define CL_INVALID_VALUE (-30)
#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
#define CL_DEVICE_TYPE_GPU (1 << 2)
#define CL_DEVICE_TYPE_CPU (1 << 1)
#define CL_MEM_READ_ONLY (1 << 2)
#define CL_MEM_WRITE_ONLY (1 << 1)
#define CL_MEM_COPY_HOST_PTR (1 << 5)

#define CL_PLATFORM_NAME 0x0902
#define CL_DEVICE_NAME 0x102B
#define CL_DEVICE_VERSION 0x102F
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x101F
#define CL_DEVICE_MAX_WORK_GROUP_SIZE 0x1004
#define CL_DEVICE_MAX_MEM_ALLOC_SIZE 0x1010

/* --- function pointer types ---------------------------------------------- */
typedef cl_int (*clGetPlatformIDs_fn)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int (*clGetPlatformInfo_fn)(cl_platform_id, cl_uint, size_t, void *, size_t *);
typedef cl_int (*clGetDeviceIDs_fn)(cl_platform_id, cl_device_type, cl_uint, cl_device_id *, cl_uint *);
typedef cl_int (*clGetDeviceInfo_fn)(cl_device_id, cl_uint, size_t, void *, size_t *);
typedef cl_context (*clCreateContext_fn)(const cl_context_properties *, cl_uint,
                                         const cl_device_id *, void *, void *, cl_int *);
typedef cl_command_queue (*clCreateCommandQueue_fn)(cl_context, cl_device_id,
                                                    cl_command_queue_properties, cl_int *);
typedef cl_program (*clCreateProgramWithSource_fn)(cl_context, cl_uint,
                                                   const char **, const size_t *, cl_int *);
typedef cl_int (*clBuildProgram_fn)(cl_program, cl_uint, const cl_device_id *,
                                    const char *, void *, void *);
typedef cl_int (*clGetProgramBuildInfo_fn)(cl_program, cl_device_id, cl_uint, size_t, void *, size_t *);
typedef cl_kernel (*clCreateKernel_fn)(cl_program, const char *, cl_int *);
typedef cl_int (*clSetKernelArg_fn)(cl_kernel, cl_uint, size_t, const void *);
typedef cl_int (*clEnqueueNDRangeKernel_fn)(cl_command_queue, cl_kernel, cl_uint,
                                            const size_t *, const size_t *,
                                            const size_t *, cl_uint, const void *, void *);
typedef cl_int (*clFinish_fn)(cl_command_queue);
typedef cl_mem (*clCreateBuffer_fn)(cl_context, cl_mem_flags, size_t, void *, cl_int *);
typedef cl_int (*clEnqueueWriteBuffer_fn)(cl_command_queue, cl_mem, cl_uint, size_t,
                                          size_t, const void *, cl_uint, const void *, void *);
typedef cl_int (*clEnqueueReadBuffer_fn)(cl_command_queue, cl_mem, cl_uint, size_t,
                                         size_t, void *, cl_uint, const void *, void *);
typedef cl_int (*clReleaseMemObject_fn)(cl_mem);
typedef cl_int (*clReleaseKernel_fn)(cl_kernel);
typedef cl_int (*clReleaseProgram_fn)(cl_program);
typedef cl_int (*clReleaseCommandQueue_fn)(cl_command_queue);
typedef cl_int (*clReleaseContext_fn)(cl_context);

#define CL_PROGRAM_BUILD_LOG 0x1183

typedef struct ws_cl_api {
    clGetPlatformIDs_fn get_platform_ids;
    clGetPlatformInfo_fn get_platform_info;
    clGetDeviceIDs_fn get_device_ids;
    clGetDeviceInfo_fn get_device_info;
    clCreateContext_fn create_context;
    clCreateCommandQueue_fn create_command_queue;
    clCreateProgramWithSource_fn create_program_with_source;
    clBuildProgram_fn build_program;
    clGetProgramBuildInfo_fn get_program_build_info;
    clCreateKernel_fn create_kernel;
    clSetKernelArg_fn set_kernel_arg;
    clEnqueueNDRangeKernel_fn enqueue_ndrange_kernel;
    clFinish_fn finish;
    clCreateBuffer_fn create_buffer;
    clEnqueueWriteBuffer_fn enqueue_write_buffer;
    clEnqueueReadBuffer_fn enqueue_read_buffer;
    clReleaseMemObject_fn release_mem_object;
    clReleaseKernel_fn release_kernel;
    clReleaseProgram_fn release_program;
    clReleaseCommandQueue_fn release_command_queue;
    clReleaseContext_fn release_context;
} ws_cl_api;

struct ws_opencl {
    ws_dynlib *lib;
    ws_cl_api api;
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    ws_opencl_info info;
    int available;
    int device_lost;
};

/* ISO C forbids object->function pointer casts; a union of the two pointer
 * kinds is the standard-conformant, implementation-defined way (platform-only,
 * documented in ws_opencl_probe.c). */
#define WS_CL_LOAD(field, sym)                                                   \
    do {                                                                         \
        union { void *obj; void *fn; } u_;                                       \
        u_.obj = ws_dynlib_symbol(cl->lib, sym);                                 \
        memcpy(&cl->api.field, &u_.fn, sizeof(cl->api.field));                   \
        if (cl->api.field == NULL) {                                             \
            ws_dynlib_close(cl->lib);                                            \
            cl->lib = NULL;                                                      \
            return WS_ERR_UNSUPPORTED;                                           \
        }                                                                        \
    } while (0)

static ws_result load_api(ws_opencl *cl)
{
    static const char *const candidates[] = {
#if defined(_WIN32)
        "OpenCL.dll",
        "opencl.dll"
#else
        "libOpenCL.so.1",
        "libOpenCL.so"
#endif
    };
    if (ws_dynlib_open_any(candidates, sizeof(candidates) / sizeof(candidates[0]),
                           &cl->lib) != WS_OK) {
        return WS_ERR_UNSUPPORTED;
    }
    WS_CL_LOAD(get_platform_ids, "clGetPlatformIDs");
    WS_CL_LOAD(get_platform_info, "clGetPlatformInfo");
    WS_CL_LOAD(get_device_ids, "clGetDeviceIDs");
    WS_CL_LOAD(get_device_info, "clGetDeviceInfo");
    WS_CL_LOAD(create_context, "clCreateContext");
    WS_CL_LOAD(create_command_queue, "clCreateCommandQueue");
    WS_CL_LOAD(create_program_with_source, "clCreateProgramWithSource");
    WS_CL_LOAD(build_program, "clBuildProgram");
    WS_CL_LOAD(get_program_build_info, "clGetProgramBuildInfo");
    WS_CL_LOAD(create_kernel, "clCreateKernel");
    WS_CL_LOAD(set_kernel_arg, "clSetKernelArg");
    WS_CL_LOAD(enqueue_ndrange_kernel, "clEnqueueNDRangeKernel");
    WS_CL_LOAD(finish, "clFinish");
    WS_CL_LOAD(create_buffer, "clCreateBuffer");
    WS_CL_LOAD(enqueue_write_buffer, "clEnqueueWriteBuffer");
    WS_CL_LOAD(enqueue_read_buffer, "clEnqueueReadBuffer");
    WS_CL_LOAD(release_mem_object, "clReleaseMemObject");
    WS_CL_LOAD(release_kernel, "clReleaseKernel");
    WS_CL_LOAD(release_program, "clReleaseProgram");
    WS_CL_LOAD(release_command_queue, "clReleaseCommandQueue");
    WS_CL_LOAD(release_context, "clReleaseContext");
    return WS_OK;
}

/* Pick a GPU device when present, else a CPU OpenCL device; fill cl->info. */
static ws_result pick_device(ws_opencl *cl)
{
    cl_uint platform_count = 0;
    if (cl->api.get_platform_ids(0u, NULL, &platform_count) != CL_SUCCESS || platform_count == 0u) {
        return WS_ERR_UNSUPPORTED;
    }
    cl_platform_id platforms[16];
    cl_uint got = platform_count > 16u ? 16u : platform_count;
    if (cl->api.get_platform_ids(got, platforms, NULL) != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }

    const cl_device_type order[2] = {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_CPU};
    for (int pass = 0; pass < 2; ++pass) {
        for (cl_uint i = 0; i < got; ++i) {
            cl_device_id dev = NULL;
            cl_uint n = 0;
            if (cl->api.get_device_ids(platforms[i], order[pass], 1u, &dev, &n) != CL_SUCCESS ||
                n == 0u || dev == NULL) {
                continue;
            }
            cl->platform = platforms[i];
            cl->device = dev;
            cl->info.runtime_loaded = 1;
            cl->info.device_found = 1;
            cl->info.platform_count = platform_count;
            cl->api.get_platform_info(platforms[i], CL_PLATFORM_NAME,
                                      sizeof(cl->info.platform_name), cl->info.platform_name, NULL);
            cl->api.get_device_info(dev, CL_DEVICE_NAME, sizeof(cl->info.device_name),
                                    cl->info.device_name, NULL);
            cl->api.get_device_info(dev, CL_DEVICE_VERSION, sizeof(cl->info.device_version),
                                    cl->info.device_version, NULL);
            cl_ulong units = 0;
            cl->api.get_device_info(dev, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(units), &units, NULL);
            cl_ulong mem = 0;
            cl->api.get_device_info(dev, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem), &mem, NULL);
            cl->info.compute_units = (uint32_t)units;
            cl->info.global_memory_bytes = (uint64_t)mem;
            return WS_OK;
        }
    }
    return WS_ERR_UNSUPPORTED;
}

static ws_result build_kernel(ws_opencl *cl)
{
    cl_int err = CL_SUCCESS;
    cl->context = cl->api.create_context(NULL, 1u, &cl->device, NULL, NULL, &err);
    if (cl->context == NULL || err != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }
    cl->queue = cl->api.create_command_queue(cl->context, cl->device, 0u, &err);
    if (cl->queue == NULL || err != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }

    const char *src = ws_kernel_ws_raster;
    size_t src_len = strlen(src);
    cl->program = cl->api.create_program_with_source(cl->context, 1u, &src, &src_len, &err);
    if (cl->program == NULL || err != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }
    err = cl->api.build_program(cl->program, 1u, &cl->device, "-cl-std=CL1.2", NULL, NULL);
    if (err != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }
    cl->kernel = cl->api.create_kernel(cl->program, "ws_raster_flat", &err);
    if (cl->kernel == NULL || err != CL_SUCCESS) {
        return WS_ERR_UNSUPPORTED;
    }
    return WS_OK;
}

ws_result ws_opencl_create(ws_opencl **out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;

    ws_opencl *cl = (ws_opencl *)calloc(1u, sizeof(*cl));
    if (cl == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    if (load_api(cl) != WS_OK || pick_device(cl) != WS_OK || build_kernel(cl) != WS_OK) {
        ws_opencl_destroy(cl);
        return WS_ERR_UNSUPPORTED;
    }
    cl->available = 1;
    *out = cl;
    return WS_OK;
}

void ws_opencl_destroy(ws_opencl *cl)
{
    if (cl == NULL) {
        return;
    }
    if (cl->lib != NULL) {
        if (cl->kernel != NULL) {
            cl->api.release_kernel(cl->kernel);
        }
        if (cl->program != NULL) {
            cl->api.release_program(cl->program);
        }
        if (cl->queue != NULL) {
            cl->api.release_command_queue(cl->queue);
        }
        if (cl->context != NULL) {
            cl->api.release_context(cl->context);
        }
        ws_dynlib_close(cl->lib);
    }
    free(cl);
}

int ws_opencl_available(const ws_opencl *cl)
{
    return cl != NULL && cl->available && !cl->device_lost;
}

int ws_opencl_device_lost(const ws_opencl *cl)
{
    return cl != NULL && cl->device_lost;
}

const ws_opencl_info *ws_opencl_info_of(const ws_opencl *cl)
{
    return cl != NULL ? &cl->info : NULL;
}

/* Mark the device lost and drop the live handles; the caller must fall back. */
static void mark_lost(ws_opencl *cl)
{
    cl->device_lost = 1;
    cl->available = 0;
}

void ws_opencl_simulate_device_loss(ws_opencl *cl)
{
    if (cl == NULL) {
        return;
    }
    mark_lost(cl);
}

/* Cast away const without a -Wcast-qual diagnostic (the OpenCL API takes
 * non-const void* for host pointers that it only reads). */
static void *unconst(const void *p)
{
    union { const void *c; void *v; } u;
    u.c = p;
    return u.v;
}

ws_result ws_opencl_raster(ws_opencl *cl,
                           const float *vx, const float *vy, const float *vz,
                           const uint8_t *color, uint32_t triangle_count,
                           uint32_t width, uint32_t height,
                           uint8_t *pixels, float *depth)
{
    if (cl == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!ws_opencl_available(cl)) {
        return WS_ERR_INVALID_STATE;
    }
    if (vx == NULL || vy == NULL || vz == NULL || color == NULL ||
        pixels == NULL || depth == NULL || width == 0u || height == 0u) {
        return WS_ERR_NULL_POINTER;
    }

    const size_t pixel_count = (size_t)width * (size_t)height;
    const size_t tri_floats = (size_t)triangle_count * 3u;
    const size_t col_bytes = (size_t)triangle_count * 9u;
    cl_int err = CL_SUCCESS;

    if (triangle_count == 0u) {
        for (size_t i = 0; i < pixel_count; ++i) {
            pixels[i * 4u + 0u] = 0u;
            pixels[i * 4u + 1u] = 0u;
            pixels[i * 4u + 2u] = 0u;
            pixels[i * 4u + 3u] = 255u;
            depth[i] = 1.0e30f;
        }
        return WS_OK;
    }

    cl_mem b_vx = cl->api.create_buffer(cl->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        tri_floats * sizeof(float), unconst(vx), &err);
    cl_mem b_vy = cl->api.create_buffer(cl->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        tri_floats * sizeof(float), unconst(vy), &err);
    cl_mem b_vz = cl->api.create_buffer(cl->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        tri_floats * sizeof(float), unconst(vz), &err);
    cl_mem b_col = cl->api.create_buffer(cl->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                         col_bytes, unconst(color), &err);
    cl_mem b_out = cl->api.create_buffer(cl->context, CL_MEM_WRITE_ONLY,
                                         pixel_count * 4u, NULL, &err);
    cl_mem b_depth = cl->api.create_buffer(cl->context, CL_MEM_WRITE_ONLY,
                                           pixel_count * sizeof(float), NULL, &err);
    if (b_vx == NULL || b_vy == NULL || b_vz == NULL || b_col == NULL ||
        b_out == NULL || b_depth == NULL) {
        if (b_vx) { cl->api.release_mem_object(b_vx); }
        if (b_vy) { cl->api.release_mem_object(b_vy); }
        if (b_vz) { cl->api.release_mem_object(b_vz); }
        if (b_col) { cl->api.release_mem_object(b_col); }
        if (b_out) { cl->api.release_mem_object(b_out); }
        if (b_depth) { cl->api.release_mem_object(b_depth); }
        mark_lost(cl);
        return WS_ERR_OUT_OF_MEMORY;
    }

    cl_uint a = 0u;
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_vx);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_vy);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_vz);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_col);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_uint), &triangle_count);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_uint), &width);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_uint), &height);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_out);
    cl->api.set_kernel_arg(cl->kernel, a++, sizeof(cl_mem), &b_depth);

    size_t global = pixel_count;
    cl_int rc = cl->api.enqueue_ndrange_kernel(cl->queue, cl->kernel, 1u, NULL, &global, NULL,
                                               0u, NULL, NULL);
    if (rc == CL_SUCCESS) {
        rc = cl->api.finish(cl->queue);
    }
    if (rc == CL_SUCCESS) {
        rc = cl->api.enqueue_read_buffer(cl->queue, b_out, 1u, 0u, pixel_count * 4u, pixels,
                                         0u, NULL, NULL);
    }
    if (rc == CL_SUCCESS) {
        rc = cl->api.enqueue_read_buffer(cl->queue, b_depth, 1u, 0u,
                                         pixel_count * sizeof(float), depth, 0u, NULL, NULL);
    }
    if (rc == CL_SUCCESS) {
        rc = cl->api.finish(cl->queue);
    }

    cl->api.release_mem_object(b_vx);
    cl->api.release_mem_object(b_vy);
    cl->api.release_mem_object(b_vz);
    cl->api.release_mem_object(b_col);
    cl->api.release_mem_object(b_out);
    cl->api.release_mem_object(b_depth);

    if (rc != CL_SUCCESS) {
        mark_lost(cl);
        return WS_ERR_UNSUPPORTED;
    }
    return WS_OK;
}
