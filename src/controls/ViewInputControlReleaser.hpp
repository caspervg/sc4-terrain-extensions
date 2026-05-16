#pragma once

#include "StatefulDragViewInputControl.hpp"

struct ViewInputControlReleaser {
    void operator()(StatefulDragViewInputControl* control) const noexcept {
        if (control) {
            control->Release();
        }
    }
};
