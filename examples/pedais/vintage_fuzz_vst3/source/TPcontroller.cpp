//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"

#include "CV_DSP/Guitar/Pedals/VintageFuzzDSP.hpp"
#include "base/source/fstreamer.h"
#include "examples/pedais/common/PedalVST3ParameterAdapter.hpp"

#if VINTAGE_FUZZ_VST3_ENABLE_CV_GUI
#include "CV_GUI/ImGuiBackend.hpp"
#endif

using namespace Steinberg;

namespace CV {
namespace {
using DSP = cvdsp::guitar::pedals::VintageFuzzDSP<float>;
}

tresult PLUGIN_API VintageFuzzVST3Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    (void)CV::Pedais::addDefinitionsToParameterContainer (parameters, DSP::getParameterDescriptors ());
    return result;
}

tresult PLUGIN_API VintageFuzzVST3Controller::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API VintageFuzzVST3Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    for (const auto& descriptor : DSP::getParameterDescriptors ())
    {
        float normalized = static_cast<float> (CV::Pedais::defaultNormalizedForDescriptor (descriptor));
        if (!streamer.readFloat (normalized))
            return kResultFalse;
        setParamNormalized (descriptor.getID (), normalized);
    }
    return kResultOk;
}

tresult PLUGIN_API VintageFuzzVST3Controller::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API VintageFuzzVST3Controller::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

IPlugView* PLUGIN_API VintageFuzzVST3Controller::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

#if VINTAGE_FUZZ_VST3_ENABLE_CV_GUI
    CV::GUI::ImGuiBackend guiBackend (Steinberg::ViewRect (0, 0, 980, 620), this);
    if (auto* view = guiBackend.createView (name))
        return view;
#endif

    return nullptr;
}

} // namespace CV
