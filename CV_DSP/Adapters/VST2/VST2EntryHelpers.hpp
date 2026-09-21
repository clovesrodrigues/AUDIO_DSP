#ifndef CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP
#define CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP

/**
 * @file VST2EntryHelpers.hpp
 * @brief Small helpers shared by per-plugin VST2 entry-point files.
 */

#include "VST2EffectBase.hpp"

#include <new>

namespace cvdsp::adapters::vst2
{

template<typename Plugin>
[[nodiscard]] AEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    if (audioMaster == nullptr)
        return nullptr;

    auto* plugin = new (std::nothrow) Plugin(audioMaster);
    if (plugin == nullptr)
        return nullptr;

    return plugin->getAeffect();
}

} // namespace cvdsp::adapters::vst2

#endif // CVDSP_ADAPTERS_VST2_ENTRYHELPERS_HPP
