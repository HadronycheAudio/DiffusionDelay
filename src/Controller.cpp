#include "Controller.h"

#include "Editor.h"
#include "Parameters.h"

#include "pluginterfaces/base/ustring.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Steinberg;

namespace DiffusionDelay {

namespace {

// A parameter whose range, curve and text all come from its ParamSpec.
class SpecParameter : public Vst::Parameter
{
public:
    SpecParameter (Vst::ParamID id, const ParamSpec& spec)
    : Vst::Parameter (UString128 (spec.title), id, UString128 (spec.units),
                      plainToNormalized (spec, spec.defaultValue),
                      spec.curve == Curve::List ? spec.labelCount - 1 : 0,
                      Vst::ParameterInfo::kCanAutomate | (spec.curve == Curve::List ? Vst::ParameterInfo::kIsList : 0)),
      _spec (spec), _id (id)
    {
    }

    void toString (Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        const double plain = normalizedToPlain (_spec, normalized);

        char text[64] {};
        if (_spec.curve == Curve::List)
            std::snprintf (text, sizeof (text), "%s", _spec.labels[static_cast<int> (plain)]);
        else if (_id == kVolume && plain <= kVolumeSilenceDb)
            std::snprintf (text, sizeof (text), "-inf");
        else
            std::snprintf (text, sizeof (text), "%.*f", _spec.precision, plain);

        UString (string, str16BufferSize (Vst::String128)).fromAscii (text);
    }

    bool fromString (const Vst::TChar* string, Vst::ParamValue& normalized) const SMTG_OVERRIDE
    {
        char text[128] {};
        UString (const_cast<Vst::TChar*> (string), tstrlen (string)).toAscii (text, sizeof (text));

        if (_spec.curve == Curve::List)
        {
            for (int i = 0; i < _spec.labelCount; ++i)
            {
                if (std::strcmp (text, _spec.labels[i]) == 0)
                {
                    normalized = plainToNormalized (_spec, i);
                    return true;
                }
            }
            return false;
        }

        if (std::strcmp (text, "-inf") == 0)
        {
            normalized = 0.0;
            return _id == kVolume;
        }

        double plain = 0.0;
        if (std::sscanf (text, "%lf", &plain) != 1 || !std::isfinite (plain))
            return false;

        normalized = plainToNormalized (_spec, plain);
        return true;
    }

    Vst::ParamValue toPlain (Vst::ParamValue normalized) const SMTG_OVERRIDE
    {
        return normalizedToPlain (_spec, normalized);
    }

    Vst::ParamValue toNormalized (Vst::ParamValue plain) const SMTG_OVERRIDE
    {
        return plainToNormalized (_spec, plain);
    }

private:
    const ParamSpec& _spec;
    Vst::ParamID _id;
};

} // namespace

tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
    const tresult result = EditController::initialize (context);
    if (result != kResultOk)
        return result;

    for (Vst::ParamID id = 0; id < kParamCount; ++id)
        parameters.addParameter (new SpecParameter (id, getParamSpec (id)));

    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    const bool ok = readParamState (state, [this] (Vst::ParamID id, double value) { setParamNormalized (id, value); });
    return ok ? kResultOk : kResultFalse;
}

IPlugView* PLUGIN_API Controller::createView (FIDString name)
{
    if (FIDStringsEqual (name, Vst::ViewType::kEditor))
        return new Editor (this);

    return nullptr;
}

} // namespace DiffusionDelay
