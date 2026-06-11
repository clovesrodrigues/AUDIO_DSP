//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/vst/vsteditcontroller.h"
#include "SoundFontEngine.h"
#include "SoundFontFolderScanner.h"

#include <filesystem>
#include <vector>

namespace CV {

class CVGMInstrumentLiteController : public Steinberg::Vst::EditControllerEx1, public Steinberg::Vst::IMidiMapping
{
public:
    CVGMInstrumentLiteController () = default;
    ~CVGMInstrumentLiteController () SMTG_OVERRIDE = default;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new CVGMInstrumentLiteController;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::Vst::ParamValue PLUGIN_API normalizedParamToPlain (Steinberg::Vst::ParamID tag,
                                                                   Steinberg::Vst::ParamValue valueNormalized) SMTG_OVERRIDE;
    Steinberg::Vst::ParamValue PLUGIN_API plainParamToNormalized (Steinberg::Vst::ParamID tag,
                                                                   Steinberg::Vst::ParamValue plainValue) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
                                                       Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment (Steinberg::int32 busIndex,
                                                                Steinberg::int16 channel,
                                                                Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                                Steinberg::Vst::ParamID& id) SMTG_OVERRIDE;

    DEFINE_INTERFACES
        DEF_INTERFACE (Steinberg::Vst::IMidiMapping)
    END_DEFINE_INTERFACES (EditController)
    DELEGATE_REFCOUNT (EditController)

private:
    void initializeDynamicLists ();
    void refreshSoundFontList ();
    void refreshPresetList ();
    void rebuildSoundFontParameterStrings ();
    void rebuildInstrumentParameterStrings ();
    std::size_t selectedSoundFontIndexFromParameter () const noexcept;
    std::size_t selectedPresetIndexFromParameter () const noexcept;

    std::filesystem::path soundFontsFolder_ {};
    std::vector<SoundFontFileInfo> soundFontFiles_ {};
    SoundFontEngine previewEngine_ {};
};

} // namespace CV
