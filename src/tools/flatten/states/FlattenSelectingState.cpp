#include "FlattenSelectingState.hpp"

#include <sstream>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"

FlattenSelectingState::FlattenSelectingState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenRenderer& renderer,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void FlattenSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    ctrl.BeginCapture();
    RebuildPreview_(ctrl, 0);
}

void FlattenSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.EndCapture();
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
    renderer_.ClearAll();
}

bool FlattenSelectingState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        return true;
    }

    if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) {
        return true;
    }

    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        dragState_.currentX = tileX;
        dragState_.currentZ = tileZ;
    }

    ctrl.TransitionTo(ControlStateId::Executing);
    return true;
}

bool FlattenSelectingState::OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t, int32_t, uint32_t) {
    ctrl.TransitionTo(ControlStateId::Hovering);
    return true;
}

bool FlattenSelectingState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t mod,
    int32_t delta) {
    if (!HandleAdjustment_(ctrl, mod, delta)) {
        return false;
    }

    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.TransitionTo(ControlStateId::Inactive);
        return true;
    }

    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl, const uint32_t modifiers) const {
    const auto preview = operation_.BuildPreview(BuildRequest_());
    if (!preview.has_value()) {
        renderer_.ClearAll();
        ctrl.ClearSelections();
        return false;
    }

    ctrl.ClearSelections();
    renderer_.Update(ctrl.GetTerrain(), *preview);
    UpdateCursor_(ctrl, *preview, modifiers);
    return true;
}

FlattenRequest FlattenSelectingState::BuildRequest_() const {
    return FlattenRequest{
        .x1 = dragState_.startX,
        .z1 = dragState_.startZ,
        .x2 = dragState_.currentX,
        .z2 = dragState_.currentZ,
        .mode = settings_.mode,
        .explicitHeight = settings_.explicitHeight.value,
        .deltaHeight = settings_.deltaHeight.value
    };
}

void FlattenSelectingState::UpdateCursor_(
    StatefulDragViewInputControl& ctrl,
    const FlattenPreview& preview,
    const uint32_t modifiers) const {
    std::ostringstream body;
    body << "Release to apply\n";
    if (settings_.mode == FlattenHeightMode::Delta) {
        body << settings_.ModeLabel() << " | " << settings_.ValueLabel();
    } else {
        body << settings_.ModeLabel() << " | target " << preview.targetHeight << "m";
    }
    body << "\n" << settings_.parameters.BuildHintText(modifiers);

    const cRZBaseString title("Flatten terrain");
    const cRZBaseString text(body.str().c_str());
    ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, text);
}

bool FlattenSelectingState::HandleAdjustment_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers,
    const int32_t delta) const {
    const auto parameter = settings_.parameters.FindByModifiers(modifiers);
    if (!parameter.has_value()) {
        return false;
    }

    parameter->AdjustByDelta(delta);
    return true;
}
