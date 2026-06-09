#pragma once

#include "public.sdk/common/pluginview.h"

#include <memory>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Steinberg::Vst {
class EditController;
}

namespace CV::GUI::ImGuiSupport {
class ImGuiLayer;
}

namespace CV::GUI::Platform::Win32 {
class Win32ChildWindow;
}

namespace CV::GUI::Renderer::OpenGL3 {
class ImGuiOpenGL3Renderer;
class OpenGL3Context;
}

namespace CV::GUI::VST3 {
class VST3ParameterBridge;
}

namespace CV::GUI {

class VST3ImGuiView final : public Steinberg::CPluginView
{
public:
    explicit VST3ImGuiView (const Steinberg::ViewRect& initialSize,
                            Steinberg::Vst::EditController* controller = nullptr);
    ~VST3ImGuiView () override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported (Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API attached (void* parent, Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API removed () override;
    Steinberg::tresult PLUGIN_API onWheel (float distance) override;
    Steinberg::tresult PLUGIN_API onKeyDown (Steinberg::char16 key,
                                             Steinberg::int16 keyMsg,
                                             Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API onKeyUp (Steinberg::char16 key,
                                           Steinberg::int16 keyMsg,
                                           Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize) override;
    Steinberg::tresult PLUGIN_API onFocus (Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API canResize () override;
    Steinberg::tresult PLUGIN_API checkSizeConstraint (Steinberg::ViewRect* rect) override;

    bool hasFocus () const noexcept { return hasFocus_; }
    VST3::VST3ParameterBridge& parameterBridge () noexcept;
    const VST3::VST3ParameterBridge& parameterBridge () const noexcept;

private:
    bool supportsPlatformType (Steinberg::FIDString type) const;
    bool attachPlatformView (void* parent);
    bool initializeImGui ();
    void shutdownImGui ();
    void removePlatformView ();
    void resizePlatformView (const Steinberg::ViewRect& size);
    void renderFrame ();
    Steinberg::tresult keyboardCaptureResult () const;
    Steinberg::tresult mouseCaptureResult () const;

#if SMTG_OS_WINDOWS
    static void renderTimerCallback (void* userData);
    static bool messageCallback (void* userData, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
#endif

    std::unique_ptr<VST3::VST3ParameterBridge> parameterBridge_;
    bool hasFocus_ {false};

#if SMTG_OS_WINDOWS
    std::unique_ptr<Platform::Win32::Win32ChildWindow> childWindow_;
    std::unique_ptr<Renderer::OpenGL3::OpenGL3Context> openGLContext_;
    std::unique_ptr<ImGuiSupport::ImGuiLayer> imguiLayer_;
    std::unique_ptr<Renderer::OpenGL3::ImGuiOpenGL3Renderer> imguiRenderer_;
    bool imguiWin32Initialized_ {false};
#endif
};

} // namespace CV::GUI
