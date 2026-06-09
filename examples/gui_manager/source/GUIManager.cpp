#include "CV_GUI/GUIManager.h"

#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

namespace CV::GUI {

const char* backendKindToString (BackendKind kind) noexcept
{
    switch (kind)
    {
        case BackendKind::Auto: return "Auto";
        case BackendKind::DearImGui: return "DearImGui";
        case BackendKind::VSTGUI: return "VSTGUI";
        case BackendKind::NativeHost: return "NativeHost";
    }
    return "NativeHost";
}

BackendKind backendKindFromString (std::string_view text) noexcept
{
    std::string lower;
    lower.reserve (text.size ());
    for (char c : text)
        lower.push_back (static_cast<char> (std::tolower (static_cast<unsigned char> (c))));

    if (lower == "imgui" || lower == "dearimgui" || lower == "dear_imgui")
        return BackendKind::DearImGui;
    if (lower == "vstgui" || lower == "vst_gui")
        return BackendKind::VSTGUI;
    if (lower == "native" || lower == "nativehost" || lower == "host")
        return BackendKind::NativeHost;
    return BackendKind::Auto;
}

GUIManager::GUIManager (BackendKind preferredBackend, EditorModel model)
: model_ (std::move (model))
, preferredBackend_ (preferredBackend)
{}

GUIManager::~GUIManager () = default;

Steinberg::IPlugView* GUIManager::createView (Steinberg::Vst::EditController* controller)
{
    const auto makeBackend = [] (BackendKind kind) -> std::unique_ptr<IGUIBackend> {
        switch (kind)
        {
            case BackendKind::DearImGui: return createImGuiBackend ();
            case BackendKind::VSTGUI: return createVSTGUIBackend ();
            case BackendKind::NativeHost: return createNativeHostBackend ();
            case BackendKind::Auto: break;
        }
        return nullptr;
    };

    std::array<BackendKind, 3> order {BackendKind::DearImGui, BackendKind::VSTGUI, BackendKind::NativeHost};
    if (preferredBackend_ != BackendKind::Auto)
        order = {preferredBackend_, BackendKind::DearImGui, BackendKind::NativeHost};

    for (auto kind : order)
    {
        auto backend = makeBackend (kind);
        if (!backend)
            continue;

        selectedBackend_ = backend->kind ();
        selectedBackendName_ = backend->name ();

        if (!backend->isAvailable ())
        {
            selectionReason_ = backend->unavailableReason ();
            continue;
        }

        selectionReason_ = "backend available";
        return backend->createView (controller, model_);
    }

    selectedBackend_ = BackendKind::NativeHost;
    selectedBackendName_ = "NativeHostBackend";
    selectionReason_ = "all custom backends unavailable; using host parameter editor";
    return nullptr;
}

} // namespace CV::GUI
