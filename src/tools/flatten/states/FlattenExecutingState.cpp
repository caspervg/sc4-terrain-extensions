#include "FlattenExecutingState.hpp"

#include "controls/StatefulDragViewInputControl.hpp"
#include "utils/Logger.h"

FlattenExecutingState::FlattenExecutingState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , dragState_(dragState) {
}

void FlattenExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    if (!operation_.Apply(BuildRequest_())) {
        LOG_WARN("FlattenExecutingState: flatten apply failed");
    }

    ctrl.TransitionTo(ControlStateId::Hovering);
}

FlattenRequest FlattenExecutingState::BuildRequest_() const {
    return FlattenRequest{
        .x1 = dragState_.startX,
        .z1 = dragState_.startZ,
        .x2 = dragState_.currentX,
        .z2 = dragState_.currentZ,
        .mode = settings_.mode,
        .explicitHeight = settings_.explicitHeight.value
    };
}
