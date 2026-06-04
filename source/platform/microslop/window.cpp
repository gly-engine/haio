#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "haio_common.hpp"
#include "window.hpp"

namespace Haio {

struct WindowsWindow::Impl {
    HWND hwnd = nullptr;
    HINSTANCE hInstance = nullptr;
    bool running = false;
};

WindowsWindow::WindowsWindow(const char* title, int width, int height)
    : title(title), width(width), height(height) {
    p = std::make_unique<Impl>();
}

WindowsWindow::~WindowsWindow() {
    shutdown();
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        WindowsWindow* window = (WindowsWindow*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }

    auto* window = (WindowsWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowsWindow::init() {
    p->hInstance = GetModuleHandle(nullptr);

    const char* CLASS_NAME = "HaioWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = Haio::WindowProc;
    wc.hInstance = p->hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    p->hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr,
        nullptr,
        p->hInstance,
        this
    );

    ShowWindow(p->hwnd, SW_SHOW);
    p->running = true;
}

void WindowsWindow::shutdown() {
    if (p->hwnd) {
        DestroyWindow(p->hwnd);
        p->hwnd = nullptr;
    }
    p->running = false;
}

void WindowsWindow::pollEvents() {
    MSG msg = {};

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT) {
            p->running = false;
        }
    }
}

bool WindowsWindow::shouldClose() const {
    return !p->running;
}

}
