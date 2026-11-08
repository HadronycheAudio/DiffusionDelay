#include "Processor.h"

#include "Controller.h"

namespace DiffusionDelay {

Processor::Processor () : RustProcessor (Controller::getClassFUID ())
{
}

} // namespace DiffusionDelay
