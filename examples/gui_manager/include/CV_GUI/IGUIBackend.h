#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "public.sdk/vst/vsteditcontroller.h"

#include <string_view>
#include <vector>

namespace CV::GUI {

enum class BackendKind
{
    Auto,
    DearImGui,
    VSTGUI,
    NativeHost
};

struct ParameterBinding
{
    Steinberg::Vst::ParamID id {};
    std::string_view label {};
    double minPlain {0.0};
    double maxPlain {1.0};
    double defaultPlain {0.0};
    std::string_view units {};
};

struct ListBoxBinding
{
    Steinberg::Vst::ParamID id {};
    std::string_view label {};
    std::vector<std::string_view> items {};
};

struct EditorModel
{
    std::string_view title {};
    ParameterBinding slider {};
    ParameterBinding knob {};
    ParameterBinding buttonCounter {};
    ListBoxBinding listbox {};
};

class IGUIBackend
{
public:
    virtual ~IGUIBackend () = default;

    virtual BackendKind kind () const noexcept = 0;
    virtual std::string_view name () const noexcept = 0;
    virtual bool isAvailable () const noexcept = 0;
    virtual std::string_view unavailableReason () const noexcept = 0;

    virtual Steinberg::IPlugView* createView (Steinberg::Vst::EditController* controller,
                                             const EditorModel& model) = 0;
};

} // namespace CV::GUI
