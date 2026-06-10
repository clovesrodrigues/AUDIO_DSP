#include "CV_GUI/ImGuiBackend.hpp"

#include "CV_GUI/VST3ImGuiView.hpp"

#include "pluginterfaces/base/fstrdefs.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace CV::GUI {
namespace {
constexpr Steinberg::int32 kDefaultWidth = 800;
constexpr Steinberg::int32 kDefaultHeight = 480;
}

ImGuiBackend::ImGuiBackend (Steinberg::Vst::EditController* controller)
: ImGuiBackend (Steinberg::ViewRect (0, 0, kDefaultWidth, kDefaultHeight), controller)
{
}

ImGuiBackend::ImGuiBackend (const Steinberg::ViewRect& initialSize, Steinberg::Vst::EditController* controller)
: initialSize_ (initialSize)
, controller_ (controller)
{
}

Steinberg::IPlugView* ImGuiBackend::createView (Steinberg::FIDString name)
{
    if (!Steinberg::FIDStringsEqual (name, Steinberg::Vst::ViewType::kEditor))
        return nullptr;

    return new VST3ImGuiView (initialSize_, controller_);
}

} // namespace CV::GUI
