#pragma once

#include "pluginterfaces/base/ftypes.h"

namespace Steinberg {
class IPlugView;
}

namespace CV::GUI {

class IGUIBackend
{
public:
    virtual ~IGUIBackend () = default;

    virtual Steinberg::IPlugView* createView (Steinberg::FIDString name) = 0;
};

} // namespace CV::GUI
