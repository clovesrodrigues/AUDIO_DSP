//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcontroller.h"
#include "TPcids.h"
#include "version.h"

#include "public.sdk/main/pluginfactory.h"

#define stringPluginName "CV Waveshaper"

using namespace Steinberg::Vst;
using namespace CV;

BEGIN_FACTORY_DEF ("Cloves Plugins",
                   "https://www.mycompanyname.com",
                   "mailto:billrods@gmail.com")

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kWaveshaperVST3ProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringPluginName,
                Vst::kDistributable,
                WaveshaperVST3Category,
                FULL_VERSION_STR,
                kVstVersionString,
                WaveshaperVST3Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kWaveshaperVST3ControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringPluginName "Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                WaveshaperVST3Controller::createInstance)

END_FACTORY
