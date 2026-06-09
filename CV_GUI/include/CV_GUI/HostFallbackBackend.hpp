#pragma once

#include "CV_GUI/IGUIBackend.hpp"

namespace CV::GUI {

class HostFallbackBackend final : public IGUIBackend
{
public:
    Steinberg::IPlugView* createView (Steinberg::FIDString name) override;
};

} // namespace CV::GUI
