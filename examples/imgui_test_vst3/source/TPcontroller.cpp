//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPcontroller.h"
#include "TPcids.h"
#include "TPimguieditor.h"

#include "public.sdk/source/vst/vstparameters.h"

using namespace Steinberg;

namespace CV {

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestController::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("ImGui Slider"), kParamGuiSlider, nullptr,
                                                      0.0, 1.0, 0.5));

    auto* list = new Vst::StringListParameter (STR16 ("ImGui Listbox"), kParamGuiListBox);
    list->appendString (STR16 ("Compressor"));
    list->appendString (STR16 ("Limiter"));
    list->appendString (STR16 ("Noise Gate"));
    list->setNormalized (0.0);
    parameters.addParameter (list);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Button Counter"), kParamButtonCounter, nullptr,
                                                      0.0, 16.0, 0.0));

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestController::terminate ()
{
    return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestController::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestController::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API ImGuiTestController::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API ImGuiTestController::createView (FIDString name)
{
    if (FIDStringsEqual (name, Vst::ViewType::kEditor))
        return new ImGuiTestEditor (this);

    return nullptr;
}

//------------------------------------------------------------------------
} // namespace CV
