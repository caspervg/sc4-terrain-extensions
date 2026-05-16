#include "FlattenExecutingState.hpp"

#include "controls/StatefulDragViewInputControl.hpp"
#include "utils/Logger.h"

namespace {
bool UsePickedReferenceTile(const FlattenSettings& settings, const FlattenDragState& dragState) noexcept {
    return settings.mode == FlattenHeightMode::ReferenceTileAverage && dragState.hasPickedReferenceTile;
}
}

FlattenExecutingState::FlattenExecutingState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , dragState_(dragState) {
}

void FlattenExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    ctrl.FireBeforeExecute();
    if (!operation_.Apply(BuildRequest_())) {
        LOG_WARN("FlattenExecutingState: flatten apply failed");
    }

    dragState_.ClearPickedReferenceTile();
    dragState_.selectionCommitted = false;
    ctrl.TransitionTo(ControlStateId::Hovering);
}

FlattenRequest FlattenExecutingState::BuildRequest_() const {
    const bool usePickedReferenceTile = UsePickedReferenceTile(settings_, dragState_);
    return FlattenRequest{
        .x1 = dragState_.startX,
        .z1 = dragState_.startZ,
        .x2 = dragState_.currentX,
        .z2 = dragState_.currentZ,
        .referenceTileX = usePickedReferenceTile ? dragState_.pickedReferenceTileX : dragState_.startX,
        .referenceTileZ = usePickedReferenceTile ? dragState_.pickedReferenceTileZ : dragState_.startZ,
        .mode = settings_.mode,
        .shape = settings_.shape,
        .explicitHeight = settings_.explicitHeight.value,
        .deltaHeight = settings_.deltaHeight.value,
        .lineThickness = settings_.lineThickness.value
    };
}
