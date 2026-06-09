#include "CV_GUI/VST3ImGuiView.hpp"

#include "pluginterfaces/base/fstrdefs.h"
#include "CV_GUI/vst3/VST3ParameterBridge.hpp"

#include "public.sdk/vst/vsteditcontroller.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#if SMTG_OS_WINDOWS
#include "CV_GUI/imgui/ImGuiLayer.hpp"
#include "CV_GUI/platform/win32/Win32ChildWindow.hpp"
#include "CV_GUI/renderer/opengl3/ImGuiOpenGL3Renderer.hpp"
#include "CV_GUI/renderer/opengl3/OpenGL3Context.hpp"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace CV::GUI {
namespace {
constexpr unsigned int kRenderIntervalMs = 33;

std::string vstStringToUtf8 (const Steinberg::Vst::TChar* text)
{
    std::string result;
    if (!text)
        return result;

    for (auto cursor = text; *cursor != 0; ++cursor)
    {
        const auto codepoint = static_cast<unsigned int> (*cursor);
        if (codepoint < 0x80)
        {
            result.push_back (static_cast<char> (codepoint));
        }
        else if (codepoint < 0x800)
        {
            result.push_back (static_cast<char> (0xC0 | (codepoint >> 6)));
            result.push_back (static_cast<char> (0x80 | (codepoint & 0x3F)));
        }
        else
        {
            result.push_back (static_cast<char> (0xE0 | (codepoint >> 12)));
            result.push_back (static_cast<char> (0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back (static_cast<char> (0x80 | (codepoint & 0x3F)));
        }
    }
    return result;
}

std::string paramValueToString (Steinberg::Vst::EditController& controller,
                                Steinberg::Vst::ParamID paramID,
                                Steinberg::Vst::ParamValue normalizedValue)
{
    Steinberg::Vst::String128 valueText {};
    if (controller.getParamStringByValue (paramID, normalizedValue, valueText) == Steinberg::kResultTrue)
        return vstStringToUtf8 (valueText);

    char fallback[32] {};
    std::snprintf (fallback, sizeof (fallback), "%.3f", normalizedValue);
    return fallback;
}

void applyParameterGesture (VST3::VST3ParameterBridge& bridge,
                            Steinberg::Vst::ParamID paramID,
                            Steinberg::Vst::ParamValue normalizedValue)
{
    normalizedValue = std::clamp (normalizedValue, 0.0, 1.0);
    const auto beginResult = bridge.beginGesture (paramID);
    if (beginResult == Steinberg::kResultTrue || beginResult == Steinberg::kResultOk)
    {
        bridge.updateGesture (paramID, normalizedValue);
        bridge.endGesture (paramID);
    }
}

void drawParameterControl (Steinberg::Vst::EditController& controller,
                           VST3::VST3ParameterBridge& bridge,
                           const Steinberg::Vst::ParameterInfo& info)
{
    if ((info.flags & Steinberg::Vst::ParameterInfo::kIsHidden) != 0)
        return;

    Steinberg::Vst::ParamValue normalizedValue = 0.0;
    if (!bridge.getParamNormalized (info.id, normalizedValue))
        return;

    const auto title = vstStringToUtf8 (info.title);
    const auto units = vstStringToUtf8 (info.units);
    const auto label = title + "##param_" + std::to_string (info.id);
    const bool readOnly = (info.flags & Steinberg::Vst::ParameterInfo::kIsReadOnly) != 0;

    if (readOnly)
        ImGui::BeginDisabled ();

    ImGui::PushID (static_cast<int> (info.id));

    if (info.stepCount > 0)
    {
        const int stepCount = static_cast<int> (info.stepCount);
        int currentStep = static_cast<int> (std::lround (std::clamp (normalizedValue, 0.0, 1.0) * stepCount));
        currentStep = std::clamp (currentStep, 0, stepCount);
        const auto preview = paramValueToString (controller, info.id, static_cast<double> (currentStep) / stepCount);

        if (ImGui::BeginCombo (label.c_str (), preview.c_str ()))
        {
            for (int step = 0; step <= stepCount; ++step)
            {
                const auto candidateNormalized = static_cast<double> (step) / stepCount;
                auto itemText = paramValueToString (controller, info.id, candidateNormalized);
                if (itemText.empty ())
                    itemText = std::to_string (step);

                const bool selected = step == currentStep;
                if (ImGui::Selectable (itemText.c_str (), selected) && !readOnly)
                    applyParameterGesture (bridge, info.id, candidateNormalized);
                if (selected)
                    ImGui::SetItemDefaultFocus ();
            }
            ImGui::EndCombo ();
        }
    }
    else
    {
        const double minPlain = controller.normalizedParamToPlain (info.id, 0.0);
        const double maxPlain = controller.normalizedParamToPlain (info.id, 1.0);
        double plainValue = controller.normalizedParamToPlain (info.id, normalizedValue);
        char format[32] {};
        std::snprintf (format, sizeof (format), "%%.2f%s%s", units.empty () ? "" : " ", units.c_str ());

        if (ImGui::SliderScalar (label.c_str (), ImGuiDataType_Double, &plainValue, &minPlain, &maxPlain, format) && !readOnly)
        {
            bridge.updateGesture (info.id, controller.plainParamToNormalized (info.id, plainValue));
        }

        if (!readOnly && ImGui::IsItemActivated ())
            bridge.beginGesture (info.id);
        if (!readOnly && ImGui::IsItemDeactivatedAfterEdit ())
            bridge.endGesture (info.id);
    }

    ImGui::SameLine ();
    ImGui::TextUnformatted (paramValueToString (controller, info.id, normalizedValue).c_str ());

    if (!readOnly)
    {
        ImGui::SameLine ();
        if (ImGui::SmallButton ("Reset"))
            applyParameterGesture (bridge, info.id, info.defaultNormalizedValue);
    }

    ImGui::PopID ();

    if (readOnly)
        ImGui::EndDisabled ();
}

void drawParameterEditor (VST3::VST3ParameterBridge& bridge)
{
    auto* controller = bridge.controller ();
    if (!controller)
    {
        ImGui::TextUnformatted ("No VST3 EditController is attached to CV_GUI.");
        return;
    }

    const int parameterCount = controller->getParameterCount ();
    if (parameterCount <= 0)
    {
        ImGui::TextUnformatted ("No editable VST3 parameters were reported by the controller.");
        return;
    }

    ImGui::TextUnformatted ("CV Compressor - Dear ImGui editor");
    ImGui::Separator ();

    for (int index = 0; index < parameterCount; ++index)
    {
        Steinberg::Vst::ParameterInfo info {};
        if (controller->getParameterInfo (index, info) == Steinberg::kResultTrue)
            drawParameterControl (*controller, bridge, info);
    }

    ImGui::Separator ();
    ImGui::TextDisabled ("Automation: beginEdit / performEdit / endEdit via VST3ParameterBridge.");
}

#if SMTG_OS_WINDOWS
bool isKeyboardMessage (UINT message)
{
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
           message == WM_CHAR || message == WM_IME_CHAR || message == WM_IME_COMPOSITION;
}

bool isMouseMessage (UINT message)
{
    return message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE || message == WM_MOUSELEAVE ||
           message == WM_NCMOUSELEAVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
           message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ||
           message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ||
           message == WM_MBUTTONDBLCLK || message == WM_XBUTTONDOWN || message == WM_XBUTTONUP ||
           message == WM_XBUTTONDBLCLK || message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
}
#endif
}

VST3ImGuiView::VST3ImGuiView (const Steinberg::ViewRect& initialSize,
                              Steinberg::Vst::EditController* controller)
: Steinberg::CPluginView (&initialSize)
, parameterBridge_ (std::make_unique<VST3::VST3ParameterBridge> (controller))
{
}

VST3ImGuiView::~VST3ImGuiView ()
{
    removePlatformView ();
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::isPlatformTypeSupported (Steinberg::FIDString type)
{
    return supportsPlatformType (type) ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::attached (void* parent, Steinberg::FIDString type)
{
    if (!parent || !supportsPlatformType (type))
        return Steinberg::kResultFalse;

    if (!attachPlatformView (parent))
        return Steinberg::kResultFalse;

    return Steinberg::CPluginView::attached (parent, type);
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::removed ()
{
    if (parameterBridge_)
        parameterBridge_->cancelActiveGesture ();
    removePlatformView ();
    hasFocus_ = false;
    return Steinberg::CPluginView::removed ();
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::onWheel (float /*distance*/)
{
    return mouseCaptureResult ();
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::onKeyDown (Steinberg::char16 /*key*/, Steinberg::int16 /*keyMsg*/, Steinberg::int16 /*modifiers*/)
{
    return keyboardCaptureResult ();
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::onKeyUp (Steinberg::char16 /*key*/, Steinberg::int16 /*keyMsg*/, Steinberg::int16 /*modifiers*/)
{
    return keyboardCaptureResult ();
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::onSize (Steinberg::ViewRect* newSize)
{
    const auto result = Steinberg::CPluginView::onSize (newSize);
    if (newSize && result == Steinberg::kResultTrue)
        resizePlatformView (*newSize);
    return result;
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::onFocus (Steinberg::TBool state)
{
    hasFocus_ = state != 0;
#if SMTG_OS_WINDOWS
    if (imguiLayer_)
        imguiLayer_->addFocusEvent (hasFocus_);
#endif
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::canResize ()
{
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API VST3ImGuiView::checkSizeConstraint (Steinberg::ViewRect* /*rect*/)
{
    return Steinberg::kResultFalse;
}

VST3::VST3ParameterBridge& VST3ImGuiView::parameterBridge () noexcept
{
    return *parameterBridge_;
}

const VST3::VST3ParameterBridge& VST3ImGuiView::parameterBridge () const noexcept
{
    return *parameterBridge_;
}

bool VST3ImGuiView::supportsPlatformType (Steinberg::FIDString type) const
{
#if SMTG_OS_WINDOWS
    return Steinberg::FIDStringsEqual (type, Steinberg::kPlatformTypeHWND);
#else
    (void)type;
    return false;
#endif
}

bool VST3ImGuiView::attachPlatformView (void* parent)
{
#if SMTG_OS_WINDOWS
    removePlatformView ();

    childWindow_ = std::make_unique<Platform::Win32::Win32ChildWindow> ();
    if (!childWindow_->create (static_cast<HWND> (parent), rect.getWidth (), rect.getHeight ()))
    {
        removePlatformView ();
        return false;
    }

    openGLContext_ = std::make_unique<Renderer::OpenGL3::OpenGL3Context> ();
    if (!openGLContext_->create (childWindow_->handle ()))
    {
        removePlatformView ();
        return false;
    }

    if (!initializeImGui ())
    {
        removePlatformView ();
        return false;
    }

    childWindow_->setMessageCallback (&VST3ImGuiView::messageCallback, this);
    if (!childWindow_->startRenderTimer (kRenderIntervalMs, &VST3ImGuiView::renderTimerCallback, this))
    {
        removePlatformView ();
        return false;
    }

    renderFrame ();
    return true;
#else
    (void)parent;
    return false;
#endif
}

bool VST3ImGuiView::initializeImGui ()
{
#if SMTG_OS_WINDOWS
    if (!openGLContext_ || !openGLContext_->makeCurrent ())
        return false;

    imguiLayer_ = std::make_unique<ImGuiSupport::ImGuiLayer> ();
    if (!imguiLayer_->create ())
    {
        shutdownImGui ();
        return false;
    }

    imguiLayer_->setCurrent ();
    if (!ImGui_ImplWin32_Init (childWindow_->handle ()))
    {
        shutdownImGui ();
        return false;
    }
    imguiWin32Initialized_ = true;

    imguiRenderer_ = std::make_unique<Renderer::OpenGL3::ImGuiOpenGL3Renderer> ();
    if (!imguiRenderer_->create ())
    {
        shutdownImGui ();
        return false;
    }

    return true;
#else
    return false;
#endif
}

void VST3ImGuiView::shutdownImGui ()
{
#if SMTG_OS_WINDOWS
    if (imguiLayer_)
        imguiLayer_->setCurrent ();

    if (imguiRenderer_)
    {
        imguiRenderer_->destroy ();
        imguiRenderer_.reset ();
    }

    if (imguiWin32Initialized_)
    {
        ImGui_ImplWin32_Shutdown ();
        imguiWin32Initialized_ = false;
    }

    if (imguiLayer_)
    {
        imguiLayer_->destroy ();
        imguiLayer_.reset ();
    }
#endif
}

void VST3ImGuiView::removePlatformView ()
{
#if SMTG_OS_WINDOWS
    if (childWindow_)
        childWindow_->stopRenderTimer ();

    shutdownImGui ();

    if (openGLContext_)
    {
        openGLContext_->destroy ();
        openGLContext_.reset ();
    }

    if (childWindow_)
    {
        childWindow_->destroy ();
        childWindow_.reset ();
    }
#endif
}

void VST3ImGuiView::resizePlatformView (const Steinberg::ViewRect& size)
{
#if SMTG_OS_WINDOWS
    if (childWindow_)
        childWindow_->resize (size.getWidth (), size.getHeight ());
    renderFrame ();
#else
    (void)size;
#endif
}

void VST3ImGuiView::renderFrame ()
{
#if SMTG_OS_WINDOWS
    if (!childWindow_ || !openGLContext_ || !imguiLayer_ || !imguiRenderer_)
        return;

    if (!openGLContext_->makeCurrent ())
        return;

    imguiLayer_->setCurrent ();
    ImGui_ImplWin32_NewFrame ();
    imguiRenderer_->newFrame ();
    imguiLayer_->beginFrame ();

    ImGui::SetNextWindowPos (ImVec2 (16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize (ImVec2 (620.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin ("CV Compressor");
    drawParameterEditor (*parameterBridge_);
    ImGui::End ();

    openGLContext_->prepareFrame (rect.getWidth (), rect.getHeight ());
    imguiRenderer_->renderDrawData (imguiLayer_->render ());
    openGLContext_->swapBuffers ();
#endif
}

Steinberg::tresult VST3ImGuiView::keyboardCaptureResult () const
{
#if SMTG_OS_WINDOWS
    if (imguiLayer_ && imguiLayer_->wantsKeyboardCapture ())
        return Steinberg::kResultTrue;
#endif
    return Steinberg::kResultFalse;
}

Steinberg::tresult VST3ImGuiView::mouseCaptureResult () const
{
#if SMTG_OS_WINDOWS
    if (imguiLayer_ && imguiLayer_->wantsMouseCapture ())
        return Steinberg::kResultTrue;
#endif
    return Steinberg::kResultFalse;
}

#if SMTG_OS_WINDOWS
void VST3ImGuiView::renderTimerCallback (void* userData)
{
    auto* view = static_cast<VST3ImGuiView*> (userData);
    if (view)
        view->renderFrame ();
}

bool VST3ImGuiView::messageCallback (void* userData, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    auto* view = static_cast<VST3ImGuiView*> (userData);
    if (!view || !view->imguiLayer_)
        return false;

    view->imguiLayer_->setCurrent ();
    if (message == WM_GETDLGCODE && (view->imguiLayer_->wantsTextInput () || view->imguiLayer_->wantsKeyboardCapture ()))
    {
        result = DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTTAB;
        return true;
    }

    if (ImGui_ImplWin32_WndProcHandler (hwnd, message, wParam, lParam))
    {
        result = 1;
        return true;
    }

    if (isKeyboardMessage (message) && view->imguiLayer_->wantsKeyboardCapture ())
    {
        result = 0;
        return true;
    }

    if (isMouseMessage (message) && view->imguiLayer_->wantsMouseCapture ())
    {
        result = 0;
        return true;
    }

    return false;
}
#endif

} // namespace CV::GUI
