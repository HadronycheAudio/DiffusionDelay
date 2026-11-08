#include "Controller.h"

#include "Editor.h"

namespace DiffusionDelay {

Steinberg::IPlugView* Controller::createEditor ()
{
    return new Editor (this);
}

} // namespace DiffusionDelay
