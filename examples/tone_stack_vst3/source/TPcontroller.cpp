//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#if TONE_STACK_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {

tresult PLUGIN_API ToneStackVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Bass"), kParamToneStackBass, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Mid"), kParamToneStackMid, STR16 (""),
                                                      0.0, 1.0, 0.5));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Treble"), kParamToneStackTreble, STR16 (""),
                                                      0.0, 1.0, 0.5));
    return result;
}

tresult PLUGIN_API ToneStackVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API ToneStackVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float bass = 0.5f;
    float mid = 0.5f;
    float treble = 0.5f;

    if (!streamer.readFloat (bass) || !streamer.readFloat (mid) || !streamer.readFloat (treble))
        return kResultFalse;

    setParamNormalized (kParamToneStackBass, bass);
    setParamNormalized (kParamToneStackMid, mid);
    setParamNormalized (kParamToneStackTreble, treble);
    return kResultOk;
}

tresult PLUGIN_API ToneStackVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API ToneStackVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API ToneStackVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if TONE_STACK_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
