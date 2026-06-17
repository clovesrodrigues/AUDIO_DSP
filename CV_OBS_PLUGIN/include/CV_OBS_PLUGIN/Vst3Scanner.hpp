#ifndef CV_OBS_PLUGIN_VST3_SCANNER_HPP
#define CV_OBS_PLUGIN_VST3_SCANNER_HPP

namespace cv_obs_plugin {

/**
 * Scans standard VST3 directories for the current platform, plus the
 * obs-vst3/plugins/ folder inside the OBS module config path, and writes
 * all discovered audio-component classes to obs-vst3/cache.json.
 *
 * @return Number of audio-component entries written to the cache, or -1 on
 *         fatal error (e.g. could not obtain the OBS config path).
 */
int scanAndCacheVst3Plugins();

} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_VST3_SCANNER_HPP
