#include "TPcontroller.h"
#include "TPcids.h"

#include "CV_GUI/GUIManager.h"
#include "base/source/fstreamer.h"
#include "public.sdk/vst/vstparameters.h"

using namespace Steinberg;

namespace CV {
namespace {
constexpr double kSliderDefault = 0.5;
constexpr double kKnobDefault = 0.25;

GUI::BackendKind configuredBackend () noexcept
{
#ifdef CV_GUI_TEST_BACKEND
    return GUI::backendKindFromString (CV_GUI_TEST_BACKEND);
#else
    return GUI::BackendKind::Auto;
#endif
}

GUI::EditorModel makeEditorModel ()
{
    GUI::EditorModel model;
    model.title = "GUI Manager Test";
    model.buttonCounter = {kParamButtonCounter, "Button Counter", 0.0, 16.0, 0.0, "clicks"};
    model.slider = {kParamSlider, "Slider", 0.0, 1.0, kSliderDefault, ""};
    model.knob = {kParamKnob, "Knob", 0.0, 1.0, kKnobDefault, ""};
    model.listbox = {kParamListbox, "Listbox", {"Dear ImGui", "VSTGUI", "Native Host"}};
    return model;
}
}

GUITestController::GUITestController () = default;
GUITestController::~GUITestController () = default;

tresult PLUGIN_API GUITestController::initialize (FUnknown* context)
{
    const tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Button Counter"), kParamButtonCounter, STR16 ("clicks"),
                                                      0.0, 16.0, 0.0));
    parameters.addParameter (new Vst::RangeParameter (STR16 ("Slider"), kParamSlider, STR16 (""),
                                                      0.0, 1.0, kSliderDefault));

    auto* listbox = new Vst::StringListParameter (STR16 ("Listbox"), kParamListbox);
    listbox->appendString (STR16 ("Dear ImGui"));
    listbox->appendString (STR16 ("VSTGUI"));
    listbox->appendString (STR16 ("Native Host"));
    listbox->setNormalized (0.0);
    parameters.addParameter (listbox);

    parameters.addParameter (new Vst::RangeParameter (STR16 ("Knob"), kParamKnob, STR16 (""),
                                                      0.0, 1.0, kKnobDefault));
    return result;
}

tresult PLUGIN_API GUITestController::terminate ()
{
    return EditControllerEx1::terminate ();
}

tresult PLUGIN_API GUITestController::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    return kResultOk;
}

IPlugView* PLUGIN_API GUITestController::createView (FIDString name)
{
    if (!FIDStringsEqual (name, Vst::ViewType::kEditor))
        return nullptr;

    GUI::GUIManager manager (configuredBackend (), makeEditorModel ());
    return manager.createView (this);
}

tresult PLUGIN_API GUITestController::setState (IBStream* /*state*/)
{
    return kResultTrue;
}

tresult PLUGIN_API GUITestController::getState (IBStream* /*state*/)
{
    return kResultTrue;
}

} // namespace CV
