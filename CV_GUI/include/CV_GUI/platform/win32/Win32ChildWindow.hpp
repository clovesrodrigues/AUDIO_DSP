#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace CV::GUI::Platform::Win32 {

class Win32ChildWindow
{
public:
#if defined(_WIN32)
    using MessageCallback = bool (*) (void* userData, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
#else
    using MessageCallback = bool (*) (void* userData, void* hwnd, unsigned int message, unsigned long wParam, long lParam, long& result);
#endif
    using TimerCallback = void (*) (void* userData);

    Win32ChildWindow () = default;
    ~Win32ChildWindow ();

    Win32ChildWindow (const Win32ChildWindow&) = delete;
    Win32ChildWindow& operator= (const Win32ChildWindow&) = delete;

#if defined(_WIN32)
    bool create (HWND parent, int width, int height);
    void destroy ();
    void resize (int width, int height);
    void setMessageCallback (MessageCallback callback, void* userData);
    bool startRenderTimer (unsigned int intervalMs, TimerCallback callback, void* userData);
    void stopRenderTimer ();

    HWND handle () const noexcept { return hwnd_; }
    HWND parent () const noexcept { return parent_; }
#else
    bool create (void* parent, int width, int height);
    void destroy ();
    void resize (int width, int height);
    void setMessageCallback (MessageCallback callback, void* userData);
    bool startRenderTimer (unsigned int intervalMs, TimerCallback callback, void* userData);
    void stopRenderTimer ();

    void* handle () const noexcept { return nullptr; }
    void* parent () const noexcept { return nullptr; }
#endif

private:
#if defined(_WIN32)
    static LRESULT CALLBACK windowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool registerWindowClass (HINSTANCE instance);

    HWND hwnd_ {nullptr};
    HWND parent_ {nullptr};
    MessageCallback messageCallback_ {nullptr};
    void* messageUserData_ {nullptr};
    TimerCallback timerCallback_ {nullptr};
    void* timerUserData_ {nullptr};
    bool timerActive_ {false};
#endif
};

} // namespace CV::GUI::Platform::Win32
