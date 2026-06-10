#include "CV_GUI/HostFallbackBackend.hpp"

namespace CV::GUI {

Steinberg::IPlugView* HostFallbackBackend::createView (Steinberg::FIDString /*name*/)
{
    return nullptr;
}

} // namespace CV::GUI
