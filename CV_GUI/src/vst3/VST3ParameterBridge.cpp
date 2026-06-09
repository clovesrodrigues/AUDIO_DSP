#include "CV_GUI/vst3/VST3ParameterBridge.hpp"

#include "public.sdk/vst/vsteditcontroller.h"

namespace CV::GUI::VST3 {

VST3ParameterBridge::VST3ParameterBridge (Steinberg::Vst::EditController* controller)
: controller_ (controller)
{
}

VST3ParameterBridge::~VST3ParameterBridge ()
{
    cancelActiveGesture ();
}

void VST3ParameterBridge::setController (Steinberg::Vst::EditController* controller)
{
    if (controller_ == controller)
        return;

    cancelActiveGesture ();
    controller_ = controller;
}

bool VST3ParameterBridge::getParamNormalized (Steinberg::Vst::ParamID paramID, Steinberg::Vst::ParamValue& value) const
{
    if (!controller_)
        return false;

    value = controller_->getParamNormalized (paramID);
    return true;
}

Steinberg::tresult VST3ParameterBridge::setParamNormalized (Steinberg::Vst::ParamID paramID,
                                                            Steinberg::Vst::ParamValue value)
{
    if (!controller_)
        return Steinberg::kResultFalse;

    return controller_->setParamNormalized (paramID, value);
}

Steinberg::tresult VST3ParameterBridge::beginEdit (Steinberg::Vst::ParamID paramID)
{
    if (!controller_)
        return Steinberg::kResultFalse;

    return controller_->beginEdit (paramID);
}

Steinberg::tresult VST3ParameterBridge::performEdit (Steinberg::Vst::ParamID paramID,
                                                     Steinberg::Vst::ParamValue value)
{
    if (!controller_)
        return Steinberg::kResultFalse;

    controller_->setParamNormalized (paramID, value);
    return controller_->performEdit (paramID, value);
}

Steinberg::tresult VST3ParameterBridge::endEdit (Steinberg::Vst::ParamID paramID)
{
    if (!controller_)
        return Steinberg::kResultFalse;

    return controller_->endEdit (paramID);
}

Steinberg::tresult VST3ParameterBridge::beginGesture (Steinberg::Vst::ParamID paramID)
{
    if (editActive_ && activeParamID_ == paramID)
        return Steinberg::kResultTrue;

    cancelActiveGesture ();

    const auto result = beginEdit (paramID);
    if (result == Steinberg::kResultTrue || result == Steinberg::kResultOk)
    {
        activeParamID_ = paramID;
        editActive_ = true;
    }
    return result;
}

Steinberg::tresult VST3ParameterBridge::updateGesture (Steinberg::Vst::ParamID paramID,
                                                       Steinberg::Vst::ParamValue value)
{
    if (!editActive_ || activeParamID_ != paramID)
    {
        const auto beginResult = beginGesture (paramID);
        if (beginResult != Steinberg::kResultTrue && beginResult != Steinberg::kResultOk)
            return beginResult;
    }

    return performEdit (paramID, value);
}

Steinberg::tresult VST3ParameterBridge::endGesture (Steinberg::Vst::ParamID paramID)
{
    if (!editActive_ || activeParamID_ != paramID)
        return Steinberg::kResultFalse;

    const auto result = endEdit (paramID);
    editActive_ = false;
    activeParamID_ = 0;
    return result;
}

void VST3ParameterBridge::cancelActiveGesture ()
{
    if (editActive_)
        endEdit (activeParamID_);

    editActive_ = false;
    activeParamID_ = 0;
}

} // namespace CV::GUI::VST3
