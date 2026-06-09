#include "CV_GUI/platform/win32/Win32ChildWindow.hpp"

#if defined(_WIN32)

namespace CV::GUI::Platform::Win32 {
namespace {
constexpr wchar_t kWindowClassName[] = L"CV_GUI_ImGuiChildWindow";
constexpr UINT_PTR kRenderTimerId = 1;
}

Win32ChildWindow::~Win32ChildWindow ()
{
    destroy ();
}

bool Win32ChildWindow::create (HWND parent, int width, int height)
{
    if (!parent || hwnd_)
        return false;

    HINSTANCE instance = reinterpret_cast<HINSTANCE> (GetWindowLongPtrW (parent, GWLP_HINSTANCE));
    if (!instance)
        instance = GetModuleHandleW (nullptr);

    if (!registerWindowClass (instance))
        return false;

    parent_ = parent;
    hwnd_ = CreateWindowExW (0,
                             kWindowClassName,
                             L"CV_GUI ImGui View",
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                             0,
                             0,
                             width,
                             height,
                             parent,
                             nullptr,
                             instance,
                             this);

    if (!hwnd_)
    {
        parent_ = nullptr;
        return false;
    }

    return true;
}

void Win32ChildWindow::destroy ()
{
    stopRenderTimer ();
    if (hwnd_)
    {
        DestroyWindow (hwnd_);
        hwnd_ = nullptr;
    }
    parent_ = nullptr;
}

void Win32ChildWindow::resize (int width, int height)
{
    if (!hwnd_)
        return;

    SetWindowPos (hwnd_, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32ChildWindow::setMessageCallback (MessageCallback callback, void* userData)
{
    messageCallback_ = callback;
    messageUserData_ = userData;
}

bool Win32ChildWindow::startRenderTimer (unsigned int intervalMs, TimerCallback callback, void* userData)
{
    if (!hwnd_ || !callback)
        return false;

    stopRenderTimer ();
    timerCallback_ = callback;
    timerUserData_ = userData;
    timerActive_ = SetTimer (hwnd_, kRenderTimerId, intervalMs, nullptr) != 0;
    return timerActive_;
}

void Win32ChildWindow::stopRenderTimer ()
{
    if (hwnd_ && timerActive_)
        KillTimer (hwnd_, kRenderTimerId);

    timerActive_ = false;
    timerCallback_ = nullptr;
    timerUserData_ = nullptr;
}

LRESULT CALLBACK Win32ChildWindow::windowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*> (lParam);
        auto* window = static_cast<Win32ChildWindow*> (createStruct->lpCreateParams);
        SetWindowLongPtrW (hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (window));
        return DefWindowProcW (hwnd, message, wParam, lParam);
    }

    auto* window = reinterpret_cast<Win32ChildWindow*> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));

    if (window && window->messageCallback_)
    {
        LRESULT callbackResult = 0;
        if (window->messageCallback_ (window->messageUserData_, hwnd, message, wParam, lParam, callbackResult))
            return callbackResult;
    }

    switch (message)
    {
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
        case WM_TIMER:
            if (wParam == kRenderTimerId && window && window->timerCallback_)
            {
                window->timerCallback_ (window->timerUserData_);
                return 0;
            }
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_SIZE:
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW (hwnd, GWLP_USERDATA, 0);
            break;
        default:
            break;
    }

    return DefWindowProcW (hwnd, message, wParam, lParam);
}

bool Win32ChildWindow::registerWindowClass (HINSTANCE instance)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof (windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = &Win32ChildWindow::windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor (nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClassName;

    if (RegisterClassExW (&windowClass) != 0)
        return true;

    return GetLastError () == ERROR_CLASS_ALREADY_EXISTS;
}

} // namespace CV::GUI::Platform::Win32

#else

namespace CV::GUI::Platform::Win32 {

Win32ChildWindow::~Win32ChildWindow () = default;
bool Win32ChildWindow::create (void* /*parent*/, int /*width*/, int /*height*/) { return false; }
void Win32ChildWindow::destroy () {}
void Win32ChildWindow::resize (int /*width*/, int /*height*/) {}
void Win32ChildWindow::setMessageCallback (MessageCallback /*callback*/, void* /*userData*/) {}
bool Win32ChildWindow::startRenderTimer (unsigned int /*intervalMs*/, TimerCallback /*callback*/, void* /*userData*/) { return false; }
void Win32ChildWindow::stopRenderTimer () {}

} // namespace CV::GUI::Platform::Win32

#endif
