//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/vst/vsteditcontroller.h"
#include "imgui.h"

namespace CV {

//------------------------------------------------------------------------
//  ImGuiTestEditor
//------------------------------------------------------------------------
class ImGuiTestEditor : public Steinberg::Vst::EditorView
{
public:
    explicit ImGuiTestEditor (Steinberg::Vst::EditController* controller);
    ~ImGuiTestEditor () SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported (Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API attached (void* parent, Steinberg::FIDString type) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API removed () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canResize () SMTG_OVERRIDE { return Steinberg::kResultTrue; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint (Steinberg::ViewRect* rect) SMTG_OVERRIDE;

    void renderTestFrame () noexcept;

private:
    void syncControllerParameters () noexcept;

    ImGuiContext* imguiContext_ {nullptr};
    float sliderValue_ {0.5f};
    int listBoxIndex_ {0};
    int buttonCounter_ {0};
    bool open_ {true};
};

//------------------------------------------------------------------------
} // namespace CV
