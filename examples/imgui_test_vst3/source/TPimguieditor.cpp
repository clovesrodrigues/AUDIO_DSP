//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPimguieditor.h"
#include "TPcids.h"

#include "backends/imgui/backends/imgui_impl_null.h"
#include "pluginterfaces/gui/iplugview.h"

#include <algorithm>
#include <cstring>

namespace CV {
namespace {
constexpr Steinberg::int32 kEditorWidth = 460;
constexpr Steinberg::int32 kEditorHeight = 320;

bool isKnownPlatform (Steinberg::FIDString type) noexcept
{
#if SMTG_OS_WINDOWS
    if (std::strcmp (type, Steinberg::kPlatformTypeHWND) == 0)
        return true;
#endif
#if SMTG_OS_MACOS
    if (std::strcmp (type, Steinberg::kPlatformTypeNSView) == 0)
        return true;
#endif
#if SMTG_OS_LINUX
    if (std::strcmp (type, Steinberg::kPlatformTypeX11EmbedWindowID) == 0)
        return true;
    if (std::strcmp (type, Steinberg::kPlatformTypeWaylandSurfaceID) == 0)
        return true;
#endif
    return false;
}
}

//------------------------------------------------------------------------
ImGuiTestEditor::ImGuiTestEditor (Steinberg::Vst::EditController* controller)
: EditorView (controller, nullptr)
{
    Steinberg::ViewRect size (0, 0, kEditorWidth, kEditorHeight);
    setRect (size);
}

//------------------------------------------------------------------------
ImGuiTestEditor::~ImGuiTestEditor ()
{
    if (imguiContext_)
    {
        ImGui::SetCurrentContext (imguiContext_);
        ImGui_ImplNull_Shutdown ();
        ImGui::DestroyContext (imguiContext_);
        imguiContext_ = nullptr;
    }
}

//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API ImGuiTestEditor::isPlatformTypeSupported (Steinberg::FIDString type)
{
    return isKnownPlatform (type) ? Steinberg::kResultTrue : Steinberg::kInvalidArgument;
}

//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API ImGuiTestEditor::attached (void* parent, Steinberg::FIDString type)
{
    if (isPlatformTypeSupported (type) != Steinberg::kResultTrue)
        return Steinberg::kResultFalse;

    const auto result = EditorView::attached (parent, type);
    if (result != Steinberg::kResultOk)
        return result;

    imguiContext_ = ImGui::CreateContext ();
    ImGui::SetCurrentContext (imguiContext_);

    ImGuiIO& io = ImGui::GetIO ();
    io.DisplaySize = ImVec2 (static_cast<float> (getRect ().right - getRect ().left),
                             static_cast<float> (getRect ().bottom - getRect ().top));

    ImGui_ImplNull_Init ();
    renderTestFrame ();

    return Steinberg::kResultOk;
}

//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API ImGuiTestEditor::removed ()
{
    if (imguiContext_)
    {
        ImGui::SetCurrentContext (imguiContext_);
        ImGui_ImplNull_Shutdown ();
        ImGui::DestroyContext (imguiContext_);
        imguiContext_ = nullptr;
    }

    return EditorView::removed ();
}

//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API ImGuiTestEditor::onSize (Steinberg::ViewRect* newSize)
{
    const auto result = EditorView::onSize (newSize);
    if (newSize && imguiContext_)
    {
        ImGui::SetCurrentContext (imguiContext_);
        ImGui::GetIO ().DisplaySize = ImVec2 (static_cast<float> (newSize->right - newSize->left),
                                             static_cast<float> (newSize->bottom - newSize->top));
        renderTestFrame ();
    }
    return result;
}

//------------------------------------------------------------------------
Steinberg::tresult PLUGIN_API ImGuiTestEditor::checkSizeConstraint (Steinberg::ViewRect* rect)
{
    if (!rect)
        return Steinberg::kInvalidArgument;

    rect->right = std::max<Steinberg::int32> (rect->right, rect->left + 320);
    rect->bottom = std::max<Steinberg::int32> (rect->bottom, rect->top + 240);
    return Steinberg::kResultTrue;
}

//------------------------------------------------------------------------
void ImGuiTestEditor::renderTestFrame () noexcept
{
    if (!imguiContext_)
        return;

    ImGui::SetCurrentContext (imguiContext_);
    ImGui_ImplNull_NewFrame ();
    ImGui::NewFrame ();

    ImGui::SetNextWindowPos (ImVec2 (12.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize (ImVec2 (420.0f, 260.0f), ImGuiCond_Always);
    if (ImGui::Begin ("VST3 + Dear ImGui Test", &open_, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextUnformatted ("Dear ImGui frame generated inside a VST3 editor view.");

        if (ImGui::Button ("Button"))
        {
            ++buttonCounter_;
            syncControllerParameters ();
        }
        ImGui::SameLine ();
        ImGui::Text ("Clicks: %d", buttonCounter_);

        if (ImGui::SliderFloat ("Slider", &sliderValue_, 0.0f, 1.0f))
            syncControllerParameters ();

        const char* items[] = {"Compressor", "Limiter", "Noise Gate"};
        if (ImGui::ListBox ("Listbox", &listBoxIndex_, items, IM_ARRAYSIZE (items), 3))
            syncControllerParameters ();
    }
    ImGui::End ();

    ImGui::Render ();
}

//------------------------------------------------------------------------
void ImGuiTestEditor::syncControllerParameters () noexcept
{
    auto* editController = getController ();
    if (!editController)
        return;

    editController->setParamNormalized (kParamGuiSlider, sliderValue_);
    editController->setParamNormalized (kParamGuiListBox, listBoxIndex_ <= 0 ? 0.0 : (listBoxIndex_ == 1 ? 0.5 : 1.0));
    editController->setParamNormalized (kParamButtonCounter, std::min (1.0, buttonCounter_ / 16.0));
}

//------------------------------------------------------------------------
} // namespace CV
