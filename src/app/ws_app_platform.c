/* Worldseed Engine — platform presentation.
 *
 * On Windows this opens a Win32 window and blits the CPU-rendered pixel buffer
 * with GDI (presentation only; GDI is not the renderer). On other platforms
 * there is no window: the application is driven headless from CI.
 */
#include "ws_app.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>

#include "../render/ws_framebuffer.h"
#include "../render/ws_render.h"
#include "../render/ws_showcase.h"

typedef struct present_state {
    const ws_app_options *opts;
    ws_framebuffer fb;
    float *depth;
    uint8_t *bgra;
} present_state;

static int present_frame(HWND hwnd, present_state *st)
{
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) {
        return 0;
    }
    uint32_t w = (uint32_t)(rc.right - rc.left);
    uint32_t h = (uint32_t)(rc.bottom - rc.top);
    if (w == 0u || h == 0u) {
        return 1;
    }
    if (st->fb.width != w || st->fb.height != h) {
        ws_framebuffer_free(&st->fb);
        free(st->depth);
        free(st->bgra);
        st->depth = NULL;
        st->bgra = NULL;
        if (ws_framebuffer_create(&st->fb, w, h) != WS_OK) {
            return 0;
        }
        st->depth = malloc((size_t)w * h * sizeof(float));
        st->bgra = malloc((size_t)w * h * 4u);
        if (st->depth == NULL || st->bgra == NULL) {
            return 0;
        }
    }
    if (ws_app_render_scene(st->opts, &st->fb, st->depth) != WS_OK) {
        return 0;
    }
    size_t count = (size_t)w * h;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *s = &st->fb.pixels[i * 4u];
        st->bgra[i * 4u + 0u] = s[2];
        st->bgra[i * 4u + 1u] = s[1];
        st->bgra[i * 4u + 2u] = s[0];
        st->bgra[i * 4u + 3u] = 255u;
    }

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -(LONG)h; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC dc = GetDC(hwnd);
    if (dc == NULL) {
        return 0;
    }
    StretchDIBits(dc, 0, 0, (int)w, (int)h, 0, 0, (int)w, (int)h,
                  st->bgra, &bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, dc);
    return 1;
}

static LRESULT CALLBACK ws_app_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    present_state *st = (present_state *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    (void)lp;
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (st != NULL) {
            (void)present_frame(hwnd, st);
        }
        (void)dc;
        return 0;
    }
    case WM_SIZE:
        if (st != NULL && wp != SIZE_MINIMIZED) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int ws_app_present_win32(const ws_app_options *opts)
{
    if (opts == NULL) {
        return 2;
    }
    const wchar_t *class_name = L"WorldseedWindow";
    HINSTANCE inst = GetModuleHandleW(NULL);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ws_app_wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = class_name;
    if (RegisterClassExW(&wc) == 0) {
        fprintf(stderr, "RegisterClassEx failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    RECT rc = {0, 0, (LONG)opts->width, (LONG)opts->height};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);

    char title[128];
    snprintf(title, sizeof(title), "Worldseed Engine 0.1.0-preview - %s",
             ws_showcase_scene_id((size_t)opts->scene_index));
    wchar_t wtitle[128];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 128);

    present_state st;
    ZeroMemory(&st, sizeof(st));
    st.opts = opts;

    HWND hwnd = CreateWindowExW(0, class_name, wtitle, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                rc.right - rc.left, rc.bottom - rc.top,
                                NULL, NULL, inst, NULL);
    if (hwnd == NULL) {
        fprintf(stderr, "CreateWindowEx failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)&st);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    int rc_code = 0;
    MSG msg;
    for (;;) {
        BOOL got = GetMessageW(&msg, NULL, 0, 0);
        if (got == 0) {
            break;
        }
        if (got == -1) {
            rc_code = 1;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ws_framebuffer_free(&st.fb);
    free(st.depth);
    free(st.bgra);
    return rc_code;
}

#else /* !_WIN32 */

int ws_app_present_win32(const ws_app_options *opts)
{
    (void)opts;
    fprintf(stderr, "windowed presentation is only available on Windows; "
                    "run with --headless, --capture or --selftest\n");
    return 0;
}

#endif /* _WIN32 */
