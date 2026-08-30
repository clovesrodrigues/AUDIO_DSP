#include "GraphicEQVST2.hpp"

#include "CV_DSP/Adapters/VST2/VST2EntryHelpers.hpp"

namespace {

AEffect* createGraphicEQVST2(audioMasterCallback audioMaster)
{
    return cvdsp::adapters::vst2::createEffectInstance<CV::GraphicEQVST2>(audioMaster);
}

} // namespace

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#endif
AEffect* VSTPluginMain(audioMasterCallback audioMaster)
{
    return createGraphicEQVST2(audioMaster);
}

#if defined(_WIN32)
__declspec(dllexport)
AEffect* main(audioMasterCallback audioMaster)
{
    return VSTPluginMain(audioMaster);
}
#endif

} // extern "C"
