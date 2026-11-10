/* Worldseed Engine — live application platform presentation (P11).
 *
 * On Windows this opens a full-screen (or windowed) Win32 window and blits the
 * CPU-rendered pixel buffer with GDI every frame. GDI here is presentation
 * only, never the renderer (docs/03): the pixels come from our own C17 CPU
 * renderer, the window only shows them. Keyboard and mouse input is read from
 * the Win32 message queue and mapped to session commands and free-camera input.
 *
 * On other platforms there is no window: the live application is driven headless
 * from CI with --live-headless and --live-tour.
 */
#include "ws_live.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h> /* GET_X_LPARAM / GET_Y_LPARAM */

#include "../core/ws_result.h"
#include "../platform/ws_clock.h"
#include "../render/ws_framebuffer.h"
#include "../ui/ws_hud.h"
#include "../ui/ws_session.h"

typedef struct live_state {
    const ws_live_options *opts;
    ws_session *session;
    ws_framebuffer fb;
    float *depth;
    uint8_t *bgra;
    int running;
    int fullscreen;
    int have_cursor;
    POINT last_cursor;
    double last_time_s;
} live_state;

static void toggle_fullscreen(HWND hwnd, live_state *st)
{
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((style & WS_POPUP) != 0u) {
        SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        ShowWindow(hwnd, SW_NORMAL);
        st->fullscreen = 0;
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                     GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
        st->fullscreen = 1;
    }
}

static void handle_key(live_state *st, WPARAM key)
{
    ws_session *s = st->session;
    switch (key) {
    case VK_ESCAPE: st->running = 0; break;
    case 'Q': (void)ws_session_apply_command(s, WS_CMD_QUIT, 0.0); st->running = 0; break;
    case VK_SPACE: (void)ws_session_apply_command(s, WS_CMD_TOGGLE_PAUSE, 0.0); break;
    case VK_OEM_PLUS: case VK_ADD: (void)ws_session_apply_command(s, WS_CMD_SPEED_UP, 0.0); break;
    case VK_OEM_MINUS: case VK_SUBTRACT: (void)ws_session_apply_command(s, WS_CMD_SPEED_DOWN, 0.0); break;
    case 'V': (void)ws_session_apply_command(s, WS_CMD_CYCLE_VIEW, 0.0); break;
    case 'N': (void)ws_session_apply_command(s, WS_CMD_SELECT_NEXT, 0.0); break;
    case 'P': (void)ws_session_apply_command(s, WS_CMD_SELECT_PREV, 0.0); break;
    case 'H': (void)ws_session_apply_command(s, WS_CMD_TOGGLE_HELP, 0.0); break;
    case 'G': (void)ws_session_apply_command(s, WS_CMD_TOGGLE_STATS, 0.0); break;
    case 'T': (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0); break;
    case 'S': (void)ws_session_apply_command(s, WS_CMD_SAVE, 0.0); break;
    case 'L': (void)ws_session_apply_command(s, WS_CMD_LOAD, 0.0); break;
    case VK_OEM_4: /* [ */
        (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY,
                                       ws_session_time_of_day_s(s) - 3600.0);
        break;
    case VK_OEM_6: /* ] */
        (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY,
                                       ws_session_time_of_day_s(s) + 3600.0);
        break;
    case VK_F11: break; /* handled as a window op in the message loop */
    default: break;
    }
}

static int live_frame(HWND hwnd, live_state *st)
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
    int scale = st->opts->render_scale;
    if (scale < 1) {
        /* Auto: keep the internal resolution inside a pixel budget so the
         * interactive CPU reference renderer stays responsive at any window
         * size. Evidence captures (headless/tour) always render full size. */
        const double budget = 640.0 * 360.0;
        double px = (double)w * (double)h;
        scale = 1;
        while (scale < 4 && px / ((double)scale * (double)scale) > budget) {
            scale += 1;
        }
    }
    uint32_t rw = w / (uint32_t)scale;
    uint32_t rh = h / (uint32_t)scale;
    if (rw == 0u) { rw = 1u; }
    if (rh == 0u) { rh = 1u; }
    if (st->fb.width != rw || st->fb.height != rh) {
        ws_framebuffer_free(&st->fb);
        free(st->depth);
        free(st->bgra);
        st->depth = NULL;
        st->bgra = NULL;
        if (ws_framebuffer_create(&st->fb, rw, rh) != WS_OK) {
            return 0;
        }
        st->depth = malloc((size_t)rw * rh * sizeof(float));
        st->bgra = malloc((size_t)w * h * 4u);
        if (st->depth == NULL || st->bgra == NULL) {
            return 0;
        }
    }

    uint64_t t0 = ws_clock_monotonic_ns();
    double now_s = ws_clock_ns_to_seconds(t0);
    double dt = st->last_time_s > 0.0 ? now_s - st->last_time_s : 1.0 / 60.0;
    st->last_time_s = now_s;
    if (dt > 0.25) { dt = 0.25; }
    if (dt < 0.0) { dt = 0.0; }

    /* Keyboard camera movement for the free view. */
    double fwd = 0.0, right = 0.0, up = 0.0;
    if (GetAsyncKeyState(VK_UP) & 0x8000) { fwd += 1.0; }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) { fwd -= 1.0; }
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) { right -= 1.0; }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { right += 1.0; }
    if (GetAsyncKeyState('W') & 0x8000) { fwd += 1.0; }
    if (GetAsyncKeyState('S') & 0x8000) { up += 1.0; }
    if (GetAsyncKeyState('A') & 0x8000) { right -= 1.0; }
    if (GetAsyncKeyState('D') & 0x8000) { right += 1.0; }
    (void)ws_session_camera_input(st->session, fwd, right, up, 0.0, 0.0, dt);

    (void)ws_session_step(st->session, dt);
    if (ws_session_render(st->session, &st->fb, st->depth) != WS_OK) {
        return 0;
    }
    ws_hud_options ho = ws_hud_options_default();
    ho.scale = (st->fb.width >= 1600u) ? 2 : 1;
    ws_hud_draw(&st->fb, st->session, &ho);

    size_t count = (size_t)w * h;
    for (size_t i = 0; i < count; ++i) {
        size_t px = i % (size_t)w;
        size_t py = i / (size_t)w;
        size_t sx = px * (size_t)rw / (size_t)w;
        size_t sy = py * (size_t)rh / (size_t)h;
        const uint8_t *s = &st->fb.pixels[(sy * (size_t)rw + sx) * 4u];
        st->bgra[i * 4u + 0u] = s[2];
        st->bgra[i * 4u + 1u] = s[1];
        st->bgra[i * 4u + 2u] = s[0];
        st->bgra[i * 4u + 3u] = 255u;
    }
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -(LONG)h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC dc = GetDC(hwnd);
    if (dc != NULL) {
        StretchDIBits(dc, 0, 0, (int)w, (int)h, 0, 0, (int)w, (int)h,
                      st->bgra, &bmi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd, dc);
    }
    double ms = ws_clock_ns_to_seconds(ws_clock_monotonic_ns() - t0) * 1000.0;
    ws_session_note_frame(st->session, ms);
    return 1;
}

static LRESULT CALLBACK live_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    live_state *st = (live_state *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CLOSE:
        if (st != NULL) { st->running = 0; }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (st != NULL) {
            if (wp == VK_F11) {
                toggle_fullscreen(hwnd, st);
            } else {
                handle_key(st, wp);
            }
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (st != NULL) {
            st->have_cursor = 1;
            st->last_cursor.x = GET_X_LPARAM(lp);
            st->last_cursor.y = GET_Y_LPARAM(lp);
        }
        return 0;
    case WM_LBUTTONUP:
        if (st != NULL) { st->have_cursor = 0; }
        return 0;
    case WM_MOUSEMOVE:
        if (st != NULL && st->have_cursor && (wp & MK_LBUTTON)) {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            double dx = (double)(mx - st->last_cursor.x);
            double dy = (double)(my - st->last_cursor.y);
            st->last_cursor.x = mx;
            st->last_cursor.y = my;
            (void)ws_session_camera_input(st->session, 0.0, 0.0, 0.0, dx, dy, 1.0 / 60.0);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int ws_live_present(const ws_live_options *opts)
{
    if (opts == NULL) {
        return 2;
    }
    const wchar_t *class_name = L"WorldseedLiveWindow";
    HINSTANCE inst = GetModuleHandleW(NULL);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = live_wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = class_name;
    if (RegisterClassExW(&wc) == 0) {
        fprintf(stderr, "live: RegisterClassEx failed: %lu\n",
                (unsigned long)GetLastError());
        return 1;
    }

    live_state st;
    ZeroMemory(&st, sizeof(st));
    st.opts = opts;
    st.running = 1;
    if (ws_live_make_session(opts, &st.session) != WS_OK) {
        fprintf(stderr, "live: session create failed\n");
        return 1;
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    DWORD style = WS_OVERLAPPEDWINDOW;
    int ww = (int)opts->width;
    int wh = (int)opts->height;
    if (opts->fullscreen) {
        style = WS_POPUP | WS_VISIBLE;
        ww = screen_w;
        wh = screen_h;
        st.fullscreen = 1;
    }

    HWND hwnd = CreateWindowExW(0, class_name, L"Worldseed Engine — live world",
                                style, 0, 0, ww, wh, NULL, NULL, inst, NULL);
    if (hwnd == NULL) {
        fprintf(stderr, "live: CreateWindowEx failed: %lu\n",
                (unsigned long)GetLastError());
        ws_session_destroy(st.session);
        return 1;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)&st);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    st.last_time_s = 0.0;
    int rc_code = 0;
    while (st.running) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                st.running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!st.running) {
            break;
        }
        if (!live_frame(hwnd, &st)) {
            rc_code = 1;
            break;
        }
        /* Yield a little so the loop does not spin a core at 100%. */
        Sleep(1);
    }

    ws_framebuffer_free(&st.fb);
    free(st.depth);
    free(st.bgra);
    ws_session_destroy(st.session);
    return rc_code;
}

#else /* !_WIN32 */

int ws_live_present(const ws_live_options *opts)
{
    (void)opts;
    fprintf(stderr, "live windowed presentation is only available on Windows; "
                    "run with --live-headless or --live-tour\n");
    return 0;
}

#endif /* _WIN32 */
