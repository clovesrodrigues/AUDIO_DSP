#include "CV_GUI/GUIManager.h"

#include "imgui.h"
#include "imgui_impl_null.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/vst/vsteditcontroller.h"

#include <algorithm>
#include <cstring>

namespace CV::GUI {
namespace {
constexpr Steinberg::int32 kEditorWidth = 500;
constexpr Steinberg::int32 kEditorHeight = 340;

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

class ImGuiEditorView final : public Steinberg::Vst::EditorView
{
public:
    ImGuiEditorView (Steinberg::Vst::EditController* controller, EditorModel model)
    : EditorView (controller, nullptr)
    , model_ (std::move (model))
    {
        Steinberg::ViewRect size (0, 0, kEditorWidth, kEditorHeight);
        setRect (size);
    }

    ~ImGuiEditorView () SMTG_OVERRIDE { shutdown (); }

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported (Steinberg::FIDString type) SMTG_OVERRIDE
    {
        return isKnownPlatform (type) ? Steinberg::kResultTrue : Steinberg::kInvalidArgument;
    }

    Steinberg::tresult PLUGIN_API attached (void* parent, Steinberg::FIDString type) SMTG_OVERRIDE
    {
        if (isPlatformTypeSupported (type) != Steinberg::kResultTrue)
            return Steinberg::kResultFalse;

        const auto result = EditorView::attached (parent, type);
        if (result != Steinberg::kResultOk)
            return result;

        imguiContext_ = ImGui::CreateContext ();
        ImGui::SetCurrentContext (imguiContext_);
        ImGui::GetIO ().DisplaySize = ImVec2 (static_cast<float> (getRect ().right - getRect ().left),
                                             static_cast<float> (getRect ().bottom - getRect ().top));
        ImGui_ImplNull_Init ();
        renderFrame ();
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API removed () SMTG_OVERRIDE
    {
        shutdown ();
        return EditorView::removed ();
    }

    Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize) SMTG_OVERRIDE
    {
        const auto result = EditorView::onSize (newSize);
        if (newSize && imguiContext_)
        {
            ImGui::SetCurrentContext (imguiContext_);
            ImGui::GetIO ().DisplaySize = ImVec2 (static_cast<float> (newSize->right - newSize->left),
                                                 static_cast<float> (newSize->bottom - newSize->top));
            renderFrame ();
        }
        return result;
    }

    Steinberg::tresult PLUGIN_API canResize () SMTG_OVERRIDE { return Steinberg::kResultTrue; }

    Steinberg::tresult PLUGIN_API checkSizeConstraint (Steinberg::ViewRect* rect) SMTG_OVERRIDE
    {
        if (!rect)
            return Steinberg::kInvalidArgument;
        rect->right = std::max<Steinberg::int32> (rect->right, rect->left + 360);
        rect->bottom = std::max<Steinberg::int32> (rect->bottom, rect->top + 240);
        return Steinberg::kResultTrue;
    }

private:
    void renderFrame () noexcept
    {
        if (!imguiContext_)
            return;

        ImGui::SetCurrentContext (imguiContext_);
        ImGui_ImplNull_NewFrame ();
        ImGui::NewFrame ();
        ImGui::SetNextWindowPos (ImVec2 (12.0f, 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize (ImVec2 (460.0f, 290.0f), ImGuiCond_Always);

        bool open = true;
        if (ImGui::Begin (model_.title.data (), &open,
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            ImGui::TextUnformatted ("GUIManager / Dear ImGui backend");
            if (ImGui::Button ("Button"))
            {
                ++buttonCounter_;
                setParameterNormalized (model_.buttonCounter.id,
                                        std::min (1.0, static_cast<double> (buttonCounter_) / model_.buttonCounter.maxPlain));
            }
            if (ImGui::SliderFloat (model_.slider.label.data (), &sliderValue_,
                                    static_cast<float> (model_.slider.minPlain),
                                    static_cast<float> (model_.slider.maxPlain)))
            {
                setParameterNormalized (model_.slider.id, sliderValue_);
            }
            if (ImGui::SliderFloat (model_.knob.label.data (), &knobValue_,
                                    static_cast<float> (model_.knob.minPlain),
                                    static_cast<float> (model_.knob.maxPlain)))
            {
                setParameterNormalized (model_.knob.id, knobValue_);
            }
            const char* items[] = {"Dear ImGui", "VSTGUI", "Native Host"};
            if (ImGui::ListBox (model_.listbox.label.data (), &listBoxIndex_, items, 3))
            {
                setParameterNormalized (model_.listbox.id, static_cast<double> (listBoxIndex_) / 2.0);
            }
            ImGui::Text ("Button count: %d", buttonCounter_);
        }
        ImGui::End ();
        ImGui::Render ();
    }

    void setParameterNormalized (Steinberg::Vst::ParamID id, double normalized) noexcept
    {
        if (auto* editController = getController ())
        {
            editController->beginEdit (id);
            editController->performEdit (id, normalized);
            editController->endEdit (id);
        }
    }

    void shutdown () noexcept
    {
        if (!imguiContext_)
            return;
        ImGui::SetCurrentContext (imguiContext_);
        ImGui_ImplNull_Shutdown ();
        ImGui::DestroyContext (imguiContext_);
        imguiContext_ = nullptr;
    }

    EditorModel model_ {};
    ImGuiContext* imguiContext_ {nullptr};
    float sliderValue_ {0.5f};
    float knobValue_ {0.25f};
    int listBoxIndex_ {0};
    int buttonCounter_ {0};
};

class ImGuiBackend final : public IGUIBackend
{
public:
    BackendKind kind () const noexcept override { return BackendKind::DearImGui; }
    std::string_view name () const noexcept override { return "ImGuiBackend"; }
    bool isAvailable () const noexcept override { return true; }
    std::string_view unavailableReason () const noexcept override { return {}; }

    Steinberg::IPlugView* createView (Steinberg::Vst::EditController* controller,
                                      const EditorModel& model) override
    {
        return new ImGuiEditorView (controller, model);
    }
};
}

std::unique_ptr<IGUIBackend> createImGuiBackend ()
{
    return std::make_unique<ImGuiBackend> ();
}

} // namespace CV::GUI
