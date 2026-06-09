#include "TPprocessor.h"
#include "TPcontroller.h"
#include "TPcids.h"
#include "version.h"

#include "public.sdk/main/pluginfactory.h"

#define stringPluginName "CV GUI Manager Test"

using namespace Steinberg::Vst;
using namespace CV;

BEGIN_FACTORY_DEF ("Cloves Plugins",
                   "https://www.mycompanyname.com",
                   "mailto:billrods@gmail.com")

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kGUITestProcessorUID),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                stringPluginName,
                Vst::kDistributable,
                GUITestVST3Category,
                FULL_VERSION_STR,
                kVstVersionString,
                GUITestProcessor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (kGUITestControllerUID),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                stringPluginName "Controller",
                0,
                "",
                FULL_VERSION_STR,
                kVstVersionString,
                GUITestController::createInstance)

END_FACTORY
