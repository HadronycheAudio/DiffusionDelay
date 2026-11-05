// VST3 module entry: registers the processor and controller classes with the host.

#include "Controller.h"
#include "PluginConfig.h"
#include "Processor.h"

#include "public.sdk/source/main/pluginfactory.h"

BEGIN_FACTORY_DEF (DIFFUSIONDELAY_VENDOR, DIFFUSIONDELAY_URL, DIFFUSIONDELAY_EMAIL)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (DiffusionDelay::Processor::getClassFUID ()),
                PClassInfo::kManyInstances,
                kVstAudioEffectClass,
                DIFFUSIONDELAY_NAME,
                Vst::kDistributable,
                Vst::PlugType::kFx,
                DIFFUSIONDELAY_VERSION,
                kVstVersionString,
                DiffusionDelay::Processor::createInstance)

    DEF_CLASS2 (INLINE_UID_FROM_FUID (DiffusionDelay::Controller::getClassFUID ()),
                PClassInfo::kManyInstances,
                kVstComponentControllerClass,
                DIFFUSIONDELAY_NAME,
                0,
                "",
                DIFFUSIONDELAY_VERSION,
                kVstVersionString,
                DiffusionDelay::Controller::createInstance)

END_FACTORY
