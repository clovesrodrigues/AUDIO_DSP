#ifndef CV_OBS_PLUGIN_VST3_PARAMETER_INFO_HPP
#define CV_OBS_PLUGIN_VST3_PARAMETER_INFO_HPP

#include <cstdint>
#include <string>

#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace cv_obs_plugin {

struct CachedParameterInfo {
    Steinberg::Vst::ParamID id               = 0;
    std::string             title;
    std::string             units;
    std::int32_t            stepCount         = 0;
    double                  defaultNormalized = 0.0;
    std::int32_t            flags             = 0;
};

namespace detail {

inline std::string tcharToUtf8(const Steinberg::Vst::TChar* text) {
    std::string result;
    if (!text) return result;
    for (const auto* p = text; *p; ++p) {
        const auto c = static_cast<unsigned>(static_cast<char16_t>(*p));
        if      (c < 0x80)  { result.push_back(static_cast<char>(c)); }
        else if (c < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (c >> 6)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | (c >> 12)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return result;
}

} // namespace detail
} // namespace cv_obs_plugin

#endif // CV_OBS_PLUGIN_VST3_PARAMETER_INFO_HPP
