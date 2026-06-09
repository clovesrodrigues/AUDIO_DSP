#include "CV_GUI/GUIManager.h"

namespace CV::GUI {
namespace {
class VSTGUIBackend final : public IGUIBackend
{
public:
    BackendKind kind () const noexcept override { return BackendKind::VSTGUI; }
    std::string_view name () const noexcept override { return "VSTGUIBackend"; }

    bool isAvailable () const noexcept override
    {
#ifdef CV_GUI_ENABLE_VSTGUI_BACKEND
        return true;
#else
        return false;
#endif
    }

    std::string_view unavailableReason () const noexcept override
    {
#ifdef CV_GUI_ENABLE_VSTGUI_BACKEND
        return {};
#else
        return "VSTGUI headers exist in backends/vst3sdk/vstgui4, but this lightweight GUIManager build does not link the full VSTGUI runtime target yet";
#endif
    }

    Steinberg::IPlugView* createView (Steinberg::Vst::EditController* /*controller*/,
                                      const EditorModel& /*model*/) override
    {
        // Native host fallback until the full VSTGUI runtime is linked by an example.
        return nullptr;
    }
};
}

std::unique_ptr<IGUIBackend> createVSTGUIBackend ()
{
    return std::make_unique<VSTGUIBackend> ();
}

} // namespace CV::GUI
