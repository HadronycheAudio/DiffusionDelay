#pragma once

#include "Parameters.h"
#include "PluginConfig.h"
#include "dsp/DiffusionLoop.h"

#include "hadronyche/dsp/Smoother.h"
#include "hadronyche/vst/StereoProcessor.h"

#include <cstdint>

namespace DiffusionDelay {

// The VST3 audio component, which runs the signal path described in docs/signal-path.md.
class Processor : public Hadronyche::StereoProcessor<kParamCount>
{
public:
    Processor ();

    static const Steinberg::FUID getClassFUID () { return Steinberg::FUID (DIFFUSIONDELAY_PROCESSOR_UID); }
    static Steinberg::FUnknown* createInstance (void*)
    {
        return static_cast<Steinberg::Vst::IAudioProcessor*> (new Processor);
    }

    Steinberg::uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;
    Steinberg::uint32 PLUGIN_API getTailSamples () SMTG_OVERRIDE;

private:
    void prepareDsp () override;
    void renderBlock (Steinberg::Vst::ProcessData& data, Steinberg::Vst::AudioBusBuffers& output) override;

    double plain (ParamId id) const;
    double getTimeMs () const;
    void applyParameters ();
    void snapSmoothers ();
    void updateControls ();
    void render (const float* inputLeft, const float* inputRight, float* outputLeft, float* outputRight, Steinberg::int32 count);

    using Smoother = Hadronyche::Smoother;

    DiffusionLoop _loop;
    int _timeSamples { 0 };
    int _controlCountdown { 0 };

    // The last tempo the host gave, for hosts that don't always give one. Audio thread only, once active.
    double _tempo { 120.0 };

    // Per control tick.
    Smoother _diffusion;
    Smoother _damperGain;

    // Per sample.
    Smoother _feedback;
    Smoother _dryGain;
    Smoother _wetGain;
    Smoother _volume;

    // With nothing left to hear, and nothing left in the loop, the plug-in stops working.
    int64_t _silentSamples { 0 };
    bool _isFirstBlock { true };
};

} // namespace DiffusionDelay
