#include "CV_GUI/renderer/opengl3/OpenGL3Context.hpp"

#if defined(_WIN32)
#include <gl/GL.h>

namespace CV::GUI::Renderer::OpenGL3 {

OpenGL3Context::~OpenGL3Context ()
{
    destroy ();
}

bool OpenGL3Context::create (HWND hwnd)
{
    if (!hwnd || renderingContext_)
        return false;

    hwnd_ = hwnd;
    deviceContext_ = GetDC (hwnd_);
    if (!deviceContext_)
    {
        hwnd_ = nullptr;
        return false;
    }

    PIXELFORMATDESCRIPTOR descriptor = {};
    descriptor.nSize = sizeof (descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    descriptor.cStencilBits = 8;
    descriptor.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat (deviceContext_, &descriptor);
    if (pixelFormat == 0 || SetPixelFormat (deviceContext_, pixelFormat, &descriptor) == FALSE)
    {
        destroy ();
        return false;
    }

    renderingContext_ = wglCreateContext (deviceContext_);
    if (!renderingContext_)
    {
        destroy ();
        return false;
    }

    return makeCurrent ();
}

void OpenGL3Context::destroy ()
{
    if (renderingContext_)
    {
        if (wglGetCurrentContext () == renderingContext_)
            wglMakeCurrent (nullptr, nullptr);

        wglDeleteContext (renderingContext_);
        renderingContext_ = nullptr;
    }

    if (deviceContext_)
    {
        ReleaseDC (hwnd_, deviceContext_);
        deviceContext_ = nullptr;
    }
    hwnd_ = nullptr;
}

bool OpenGL3Context::makeCurrent () const
{
    return deviceContext_ && renderingContext_ && wglMakeCurrent (deviceContext_, renderingContext_) == TRUE;
}

void OpenGL3Context::clearCurrent () const
{
    if (wglGetCurrentContext () == renderingContext_)
        wglMakeCurrent (nullptr, nullptr);
}

void OpenGL3Context::prepareFrame (int width, int height) const
{
    if (!isValid ())
        return;

    glViewport (0, 0, width, height);
    glClearColor (0.08F, 0.08F, 0.10F, 1.0F);
    glClear (GL_COLOR_BUFFER_BIT);
}

void OpenGL3Context::swapBuffers () const
{
    if (deviceContext_)
        SwapBuffers (deviceContext_);
}

bool OpenGL3Context::isValid () const noexcept
{
    return deviceContext_ && renderingContext_;
}

} // namespace CV::GUI::Renderer::OpenGL3

#else

namespace CV::GUI::Renderer::OpenGL3 {

OpenGL3Context::~OpenGL3Context () = default;
bool OpenGL3Context::create (void* /*hwnd*/) { return false; }
void OpenGL3Context::destroy () {}
bool OpenGL3Context::makeCurrent () const { return false; }
void OpenGL3Context::clearCurrent () const {}
void OpenGL3Context::prepareFrame (int /*width*/, int /*height*/) const {}
void OpenGL3Context::swapBuffers () const {}
bool OpenGL3Context::isValid () const noexcept { return false; }

} // namespace CV::GUI::Renderer::OpenGL3

#endif
