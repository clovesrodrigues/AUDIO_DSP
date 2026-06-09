#include "CV_GUI/GUIManager.h"

namespace CV::GUI {
namespace {
class NativeHostBackend final : public IGUIBackend
{
public:
    BackendKind kind () const noexcept override { return BackendKind::NativeHost; }
    std::string_view name () const noexcept override { return "NativeHostBackend"; }
    bool isAvailable () const noexcept override { return true; }
    std::string_view unavailableReason () const noexcept override { return {}; }

    Steinberg::IPlugView* createView (Steinberg::Vst::EditController* /*controller*/,
                                      const EditorModel& /*model*/) override
    {
        return nullptr;
    }
};
}

std::unique_ptr<IGUIBackend> createNativeHostBackend ()
{
    return std::make_unique<NativeHostBackend> ();
}

} // namespace CV::GUI
