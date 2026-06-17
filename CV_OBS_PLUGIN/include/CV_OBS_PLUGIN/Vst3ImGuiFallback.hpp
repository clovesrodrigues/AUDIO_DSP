#ifndef CV_OBS_PLUGIN_VST3_IMGUI_FALLBACK_HPP
#define CV_OBS_PLUGIN_VST3_IMGUI_FALLBACK_HPP

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace Steinberg { namespace Vst { class IEditController; } }
namespace cv_obs_plugin { struct CachedParameterInfo; }

namespace cv_obs_plugin {

/**
 * Owns an ImGui parameter-editor window that runs in its own background thread.
 * Used as fallback when a VST3 plugin has no native IPlugView.
 */
struct Vst3ImGuiEditorState {
    std::string                       pluginName;
    std::vector<CachedParameterInfo>  params;
    Steinberg::Vst::IEditController*  controller = nullptr;
    std::atomic_bool                  running{false};
    std::thread                       thread;

    ~Vst3ImGuiEditorState() {
        running.store(false, std::memory_order_release);
        if (thread.joinable())
            thread.join();
    }
};

/**
 * Opens a Dear ImGui editor window for the given VST3 plugin in a background
 * thread. The caller retains ownership of @p controller — it must remain valid
 * until closeImGuiFallbackEditor() returns.
 *
 * @return Heap-allocated state (caller owns), or nullptr if unsupported /
 *         no parameters to show.
 */
Vst3ImGuiEditorState* openImGuiFallbackEditor(
    const char*                              pluginName,
    const std::vector<CachedParameterInfo>&  params,
    Steinberg::Vst::IEditController*         controller);

/**
 * Signals the editor thread to stop, joins it, and deletes the state.
 * Sets @p state to nullptr on return.
 */
void closeImGuiFallbackEditor(Vst3ImGuiEditorState*& state);

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_VST3_IMGUI_FALLBACK_HPP
