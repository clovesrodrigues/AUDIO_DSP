#pragma once

#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg::Vst {
class EditController;
}

namespace CV::GUI::VST3 {

class VST3ParameterBridge
{
public:
    explicit VST3ParameterBridge (Steinberg::Vst::EditController* controller = nullptr);
    ~VST3ParameterBridge ();

    void setController (Steinberg::Vst::EditController* controller);
    Steinberg::Vst::EditController* controller () const noexcept { return controller_; }

    bool getParamNormalized (Steinberg::Vst::ParamID paramID, Steinberg::Vst::ParamValue& value) const;
    Steinberg::tresult setParamNormalized (Steinberg::Vst::ParamID paramID, Steinberg::Vst::ParamValue value);

    Steinberg::tresult beginEdit (Steinberg::Vst::ParamID paramID);
    Steinberg::tresult performEdit (Steinberg::Vst::ParamID paramID, Steinberg::Vst::ParamValue value);
    Steinberg::tresult endEdit (Steinberg::Vst::ParamID paramID);

    Steinberg::tresult beginGesture (Steinberg::Vst::ParamID paramID);
    Steinberg::tresult updateGesture (Steinberg::Vst::ParamID paramID, Steinberg::Vst::ParamValue value);
    Steinberg::tresult endGesture (Steinberg::Vst::ParamID paramID);
    void cancelActiveGesture ();

    bool hasActiveGesture () const noexcept { return editActive_; }
    Steinberg::Vst::ParamID activeParamID () const noexcept { return activeParamID_; }

private:
    Steinberg::Vst::EditController* controller_ {nullptr};
    Steinberg::Vst::ParamID activeParamID_ {0};
    bool editActive_ {false};
};

} // namespace CV::GUI::VST3
