//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

#include <algorithm>

#if SPEC_ANALYZER_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
Vst::ParamValue normalizeEnum (float value, int maxStep)
{
    return std::clamp (static_cast<double> (value) / static_cast<double> (maxStep), 0.0, 1.0);
}
}

tresult PLUGIN_API SpectrumAnalyzerVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    auto* window = new Vst::StringListParameter (STR16 ("Windowing"), kParamSpectrumAnalyzerVST3Window);
    window->appendString (STR16 ("Rectangular"));
    window->appendString (STR16 ("Hann"));
    window->appendString (STR16 ("Hamming"));
    window->appendString (STR16 ("Blackman"));
    window->appendString (STR16 ("Blackman-Harris"));
    window->appendString (STR16 ("Kaiser"));
    window->setNormalized (normalizeEnum (1.0f, 5));
    parameters.addParameter (window);

    auto* fftSize = new Vst::StringListParameter (STR16 ("FFT Size"), kParamSpectrumAnalyzerVST3FFTSize);
    fftSize->appendString (STR16 ("512"));
    fftSize->appendString (STR16 ("1024"));
    fftSize->appendString (STR16 ("2048"));
    fftSize->setNormalized (normalizeEnum (1.0f, 2));
    parameters.addParameter (fftSize);
    return result;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API SpectrumAnalyzerVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer streamer (state, kLittleEndian);
    float window = 1.0f;
    float fftSize = 1.0f;
    if (!streamer.readFloat (window) || !streamer.readFloat (fftSize))
        return kResultFalse;
    setParamNormalized (kParamSpectrumAnalyzerVST3Window, normalizeEnum (window, 5));
    setParamNormalized (kParamSpectrumAnalyzerVST3FFTSize, normalizeEnum (fftSize, 2));
    return kResultOk;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API SpectrumAnalyzerVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API SpectrumAnalyzerVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if SPEC_ANALYZER_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
