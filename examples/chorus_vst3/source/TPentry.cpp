//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcontroller.h"
#include "TPcids.h"
#include "version.h"

#include "public.sdk/main/pluginfactory.h"

#define stringPluginName "CV Chorus"

using namespace Steinberg::Vst;
using namespace CV;

BEGIN_FACTORY_DEF ("Cloves Plugins",
                   "https://www.mycompanyname.com",
                   "mailto:billrods@gmail.com")

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kChorusVST3ProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringPluginName,
                Vst::kDistributable,
                ChorusVST3Category,
                FULL_VERSION_STR,
                kVstVersionString,
                ChorusVST3Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kChorusVST3ControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringPluginName "Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                ChorusVST3Controller::createInstance)

END_FACTORY
