//------------------------------------------------------------------------
// Copyright(c) 2026 Cloves Plugins.
//------------------------------------------------------------------------

#include "TPprocessor.h"
#include "TPcontroller.h"
#include "TPcids.h"
#include "version.h"

#include "public.sdk/main/pluginfactory.h"

#define stringPluginName "CV GM Instrument Lite"

using namespace Steinberg::Vst;
using namespace CV;

BEGIN_FACTORY_DEF ("Cloves Plugins",
                   "https://www.mycompanyname.com",
                   "mailto:billrods@gmail.com")

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kCVGMInstrumentLiteProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringPluginName,
                Vst::kDistributable,
                CVGMInstrumentLiteCategory,
                FULL_VERSION_STR,
                kVstVersionString,
                CVGMInstrumentLiteProcessor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kCVGMInstrumentLiteControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringPluginName "Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                CVGMInstrumentLiteController::createInstance)

END_FACTORY
