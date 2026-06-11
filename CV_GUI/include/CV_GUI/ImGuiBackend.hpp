#pragma once

#include "CV_GUI/IGUIBackend.hpp"
#include "pluginterfaces/gui/iplugview.h"

#include <string>

namespace Steinberg::Vst {
class EditController;
}

namespace CV::GUI {

class ImGuiBackend final : public IGUIBackend
{
public:
    explicit ImGuiBackend (Steinberg::Vst::EditController* controller = nullptr, const char* editorTitle = nullptr);
    ImGuiBackend (const Steinberg::ViewRect& initialSize,
                  Steinberg::Vst::EditController* controller = nullptr,
                  const char* editorTitle = nullptr);

    Steinberg::IPlugView* createView (Steinberg::FIDString name) override;

private:
    Steinberg::ViewRect initialSize_;
    Steinberg::Vst::EditController* controller_ {nullptr};
    std::string editorTitle_;
};

} // namespace CV::GUI
