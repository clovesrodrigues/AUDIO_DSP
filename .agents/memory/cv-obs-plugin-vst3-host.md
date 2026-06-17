---
name: CV_OBS_PLUGIN VST3 Host Architecture
description: Key decisions for the OBS VST3 host plugin — scanner, param bridging, native/ImGui editors, namespace layout, forward declarations.
---

## Namespace layout

- All code in `namespace cv_obs_plugin { namespace { ... } }` anonymous block inside `audio-dsp-vst3-filter.cpp`.
- Storage model types (`Vst3ScanStatus`, `Vst3PluginCacheEntry`, etc.) live at `cv_obs_plugin` level in `Vst3StorageModel.hpp`.
- Storage constants (`kCacheSchemaVersion`, etc.) are in `cv_obs_plugin::vst3_storage`.
- Helper types (`CachedParameterInfo`, `detail::tcharToUtf8`) live in `cv_obs_plugin::detail` inside `Vst3ParameterInfo.hpp`.

**Why:** keeps the anonymous namespace lean while giving shared headers their own stable namespaces.

## Forward declaration required in filter.cpp

`populateParameterCache(Vst3PluginInstance &)` is defined after `initializeSelectedVst3PluginLocked` calls it.
A forward declaration is placed right after `AudioDspVst3FilterState` closes (around line 574).

**Why:** C++ requires declaration before use; moving the definition earlier would entangle it with X11 window helpers above it.

## initializeVst3EditControllerLocked signature

Signature is `(Vst3PluginInstance &, MinimalVst3HostContext &, obs_source_t *)`.
The third parameter (`source`) is forwarded to `MinimalComponentHandler` so `performEdit` can call `obs_source_update_properties`.

**Why:** `state` is not in scope inside the function; `source` must be threaded through explicitly.

## OBS properties pattern for param bridging

- `getProperties()` calls `obs_properties_set_param(props, state, nullptr)` to pass filter state into callbacks.
- `onPluginSelectionModified` calls `reloadSelectedVst3Plugin` then `obs_source_update_properties` to rebuild the panel.
- `onScanPluginsClicked` calls `scanAndCacheVst3Plugins()` then repopulates the list in-place with `obs_property_list_clear` + `populateVst3PluginList`.

**Why:** matches Reaper-like behavior where the properties panel auto-refreshes after scan or plugin change.

## ImGui fallback editor

- `Vst3ImGuiFallback.cpp` runs ImGui in a dedicated thread per plugin instance.
- Win32: WGL context on a hidden HWND child window.
- Linux: GLX context on an X11 child window; events pumped manually (no imgui_impl_x11 backend exists).
- `Vst3ImGuiEditorState` destructor joins the thread; `closeImGuiFallbackEditor` also joins before nulling.

**Why:** thread-per-plugin avoids blocking the OBS audio thread; join-on-close prevents use-after-free.

## Scanner cache path

Scanner writes to `obs_module_config_path("obs-vst3/cache.json")`.
`obs_module_post_load` auto-scans only when the cache file is absent (checks with `std::filesystem::exists`).
Filter reads the same path via `resolveCachePath()`.

**Why:** avoids re-scanning on every OBS launch while still discovering plugins on first run.

## CMake structure

`Vst3Scanner.cpp`, `Vst3ImGuiFallback.cpp`, and all ImGui sources (`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_opengl3.cpp`, `backends/imgui_impl_win32.cpp`) are added to the target.
Linux links `X11::X11` + `OpenGL::GL` (and `OpenGL::GLX` if available).
Windows links `opengl32` and `imgui_impl_win32.cpp` is included.

## Bundled ImGui version

Dear ImGui 1.92.9 WIP is at `backends/imgui/`. `imgui_tables.cpp` is present (added in v1.80).
