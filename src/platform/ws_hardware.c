/* Worldseed Engine — hardware discovery (P10). */
#include "ws_hardware.h"

#include "ws_threads.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

static void detect_os(char *buf, size_t cap)
{
#if defined(_WIN32)
    const char *name = "Windows";
#elif defined(__APPLE__)
    const char *name = "macOS";
#elif defined(__linux__)
    const char *name = "Linux";
#else
    const char *name = "unknown";
#endif
    if (cap > 0u) {
        strncpy(buf, name, cap - 1u);
        buf[cap - 1u] = '\0';
    }
}

static uint64_t detect_ram_bytes(void)
{
#if defined(_WIN32)
    MEMORYSTATUSEX ms;
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        return (uint64_t)ms.ullTotalPhys;
    }
    return 0u;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return (uint64_t)pages * (uint64_t)page_size;
    }
    return 0u;
#endif
}

ws_result ws_hardware_probe(ws_hardware_info *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(out, 0, sizeof(*out));
    out->cpu_threads = ws_thread_hardware_concurrency();
    out->ram_bytes = detect_ram_bytes();
    detect_os(out->os_name, sizeof(out->os_name));
    (void)ws_opencl_probe(&out->opencl);
    return WS_OK;
}

void ws_hardware_describe(const ws_hardware_info *info, char *buf, size_t cap)
{
    if (info == NULL || buf == NULL || cap == 0u) {
        return;
    }
    double ram_gb = (double)info->ram_bytes / (1024.0 * 1024.0 * 1024.0);
    if (info->opencl.device_found) {
        snprintf(buf, cap,
                 "cpu_threads=%u ram_gb=%.1f os=%s opencl=1 platform=\"%s\" "
                 "device=\"%s\" version=\"%s\" units=%u global_mem_gb=%.1f",
                 info->cpu_threads, ram_gb, info->os_name, info->opencl.platform_name,
                 info->opencl.device_name, info->opencl.device_version,
                 info->opencl.compute_units,
                 (double)info->opencl.global_memory_bytes / (1024.0 * 1024.0 * 1024.0));
    } else {
        snprintf(buf, cap,
                 "cpu_threads=%u ram_gb=%.1f os=%s opencl=0 runtime_loaded=%d",
                 info->cpu_threads, ram_gb, info->os_name, info->opencl.runtime_loaded);
    }
}
