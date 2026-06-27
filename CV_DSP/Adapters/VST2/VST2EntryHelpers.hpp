#ifndef CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP
#define CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP

/**
 * @file VST2EntryHelpers.hpp
 * @brief Small helpers shared by per-plugin VST2 entry-point files.
 */

#include "VST2EffectBase.hpp"

namespace cvdsp::adapters::vst2
{

template<typename Plugin>
[[nodiscard]] AEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    if (audioMaster == nullptr)
        return nullptr;

    auto* plugin = new Plugin(audioMaster);
    return plugin->getAeffect();
}

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP
