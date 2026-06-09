#pragma once

#include "public.sdk/vst/vsteditcontroller.h"

namespace CV {

class GUITestController : public Steinberg::Vst::EditControllerEx1
{
public:
    GUITestController ();
    ~GUITestController () SMTG_OVERRIDE;

    static Steinberg::FUnknown* createInstance (void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new GUITestController;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

    DEFINE_INTERFACES
    END_DEFINE_INTERFACES (EditController)
    DELEGATE_REFCOUNT (EditController)
};

} // namespace CV
