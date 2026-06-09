#pragma once

#include "CV_GUI/IGUIBackend.h"

#include <memory>
#include <string_view>

namespace CV::GUI {

class GUIManager
{
public:
    GUIManager (BackendKind preferredBackend, EditorModel model);
    ~GUIManager ();

    Steinberg::IPlugView* createView (Steinberg::Vst::EditController* controller);

    BackendKind selectedBackend () const noexcept { return selectedBackend_; }
    std::string_view selectedBackendName () const noexcept { return selectedBackendName_; }
    std::string_view selectionReason () const noexcept { return selectionReason_; }

private:
    EditorModel model_ {};
    BackendKind preferredBackend_ {BackendKind::Auto};
    BackendKind selectedBackend_ {BackendKind::NativeHost};
    std::string_view selectedBackendName_ {"NativeHostBackend"};
    std::string_view selectionReason_ {"not selected yet"};
};

const char* backendKindToString (BackendKind kind) noexcept;
BackendKind backendKindFromString (std::string_view text) noexcept;

std::unique_ptr<IGUIBackend> createImGuiBackend ();
std::unique_ptr<IGUIBackend> createVSTGUIBackend ();
std::unique_ptr<IGUIBackend> createNativeHostBackend ();

} // namespace CV::GUI
