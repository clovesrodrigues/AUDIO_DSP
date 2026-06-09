#include "TPprocessor.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"

using namespace Steinberg;

namespace CV {

GUITestProcessor::GUITestProcessor ()
{
    setControllerClass (kGUITestControllerUID);
}

GUITestProcessor::~GUITestProcessor () = default;

tresult PLUGIN_API GUITestProcessor::initialize (FUnknown* context)
{
    const tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    // GUI infrastructure validation only: no DSP, no audio buses, no event bus.
    return kResultOk;
}

tresult PLUGIN_API GUITestProcessor::terminate ()
{
    return AudioEffect::terminate ();
}

tresult PLUGIN_API GUITestProcessor::setActive (TBool state)
{
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API GUITestProcessor::setupProcessing (Vst::ProcessSetup& newSetup)
{
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API GUITestProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API GUITestProcessor::process (Vst::ProcessData& /*data*/)
{
    return kResultOk;
}

tresult PLUGIN_API GUITestProcessor::setState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    return kResultOk;
}

tresult PLUGIN_API GUITestProcessor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    return kResultOk;
}

} // namespace CV
