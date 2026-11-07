#include "Controller.h"

#include "Editor.h"
#include "Parameters.h"

namespace DiffusionDelay {

Controller::Controller () : SpecController (getParamSpec, kParamCount, readParamState)
{
}

Steinberg::IPlugView* Controller::createEditor ()
{
    return new Editor (this);
}

} // namespace DiffusionDelay
