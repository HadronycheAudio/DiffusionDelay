#pragma once

#include "hadronyche/gui/PanelEditor.h"

namespace DiffusionDelay {

class Controller;

// The plug-in window: one row of controls, in signal path order.
class Editor : public Hadronyche::PanelEditor
{
public:
    explicit Editor (Controller* controller);

private:
    void buildPanel () override;
    void onSync () override;

    Controller* _controller;
};

} // namespace DiffusionDelay
