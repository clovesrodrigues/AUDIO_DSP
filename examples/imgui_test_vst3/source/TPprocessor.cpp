//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

using namespace Steinberg;

namespace CV {

//------------------------------------------------------------------------
ImGuiTestProcessor::ImGuiTestProcessor ()
{
    setControllerClass (kImGuiTestControllerUID);
}

//------------------------------------------------------------------------
ImGuiTestProcessor::~ImGuiTestProcessor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::initialize (FUnknown* context)
{
    const tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    // GUI integration test only: no DSP, no audio buses, no event bus.
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::terminate ()
{
    return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::setActive (TBool state)
{
    return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::process (Vst::ProcessData& /*data*/)
{
    // Intentionally no audio processing. The plugin exists only to validate
    // editor creation and Dear ImGui frame generation inside a VST3 view.
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
    if (symbolicSampleSize == Vst::kSample32)
        return kResultTrue;
    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestProcessor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    return kResultOk;
}

//------------------------------------------------------------------------
} // namespace CV
