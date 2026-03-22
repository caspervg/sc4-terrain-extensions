#include "FlattenHoveringState.hpp"

#include <sstream>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"

FlattenHoveringState::FlattenHoveringState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenRenderer& renderer,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void FlattenHoveringState::OnEnter(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    renderer_.ClearAll();
    UpdateHintText_(ctrl, 0);
}

void FlattenHoveringState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
}

bool FlattenHoveringState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    UpdateHoverSelection_(ctrl, x, z);
    UpdateHintText_(ctrl, mod);
    return true;
}

bool FlattenHoveringState::OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        return false;
    }

    dragState_.startX = tileX;
    dragState_.startZ = tileZ;
    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    ctrl.TransitionTo(ControlStateId::Selecting);
    return true;
}

bool FlattenHoveringState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t mod,
    int32_t delta) {
    return HandleAdjustment_(ctrl, mod, delta);
}

bool FlattenHoveringState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.TransitionTo(ControlStateId::Inactive);
        return true;
    }

    UpdateHintText_(ctrl, mod);
    return false;
}

void FlattenHoveringState::UpdateHintText_(StatefulDragViewInputControl& ctrl, const uint32_t modifiers) const {
    std::ostringstream body;
    body << "Drag to flatten\n";
    body << settings_.ModeLabel() << " | " << settings_.ValueLabel();

    const cRZBaseString title("Flatten terrain");
    const cRZBaseString text(body.str().c_str());
    ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, text);
}

void FlattenHoveringState::UpdateHoverSelection_(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z) const {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        ctrl.ClearSelections();
        return;
    }

    (void)ctrl.MarkSelected(
        tileX,
        tileZ,
        tileX,
        tileZ,
        cISTETerrain::eHilightColorType::Blue,
        true);
}

bool FlattenHoveringState::HandleAdjustment_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers,
    const int32_t delta) const {
    if (ModifierCombo{.shift = true, .ctrl = true}.Matches(modifiers)) {
        settings_.FlipDeltaSign();
        UpdateHintText_(ctrl, modifiers);
        return true;
    }

    if (ModifierCombo{.shift = true}.Matches(modifiers)) {
        settings_.CycleMode(delta);
        UpdateHintText_(ctrl, modifiers);
        return true;
    }

    const auto parameter = settings_.parameters.FindByModifiers(modifiers);
    if (!parameter.has_value()) {
        return false;
    }

    parameter->AdjustByDelta(delta);
    UpdateHintText_(ctrl, modifiers);
    return true;
}
