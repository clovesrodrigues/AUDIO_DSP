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

namespace CV::GUI::Renderer::OpenGL3 {

class OpenGL3Context
{
public:
    OpenGL3Context () = default;
    ~OpenGL3Context ();

    OpenGL3Context (const OpenGL3Context&) = delete;
    OpenGL3Context& operator= (const OpenGL3Context&) = delete;

#if defined(_WIN32)
    bool create (HWND hwnd);
#else
    bool create (void* hwnd);
#endif
    void destroy ();
    bool makeCurrent () const;
    void clearCurrent () const;
    void prepareFrame (int width, int height) const;
    void swapBuffers () const;

    bool isValid () const noexcept;

private:
#if defined(_WIN32)
    HWND hwnd_ {nullptr};
    HDC deviceContext_ {nullptr};
    HGLRC renderingContext_ {nullptr};
#endif
};

} // namespace CV::GUI::Renderer::OpenGL3
