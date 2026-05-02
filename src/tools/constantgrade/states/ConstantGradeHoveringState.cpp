#include "ConstantGradeHoveringState.hpp"

#include <format>

#include "cRZBaseString.h"
#include "cISTETerrain.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "../ConstantGradeRenderer.hpp"
#include "../ConstantGradeSettings.hpp"

ConstantGradeHoveringState::ConstantGradeHoveringState(
    ConstantGradeSettings& settings,
    ConstantGradeRenderer& renderer,
    ConstantGradeDragState& dragState)
    : settings_(settings)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void ConstantGradeHoveringState::OnEnter(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    renderer_.ClearAll();
    UpdateHintText_(ctrl, 0);
}

void ConstantGradeHoveringState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
}

bool ConstantGradeHoveringState::OnMouseMove(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    const uint32_t mod) {
    UpdateHoverSelection_(ctrl, x, z);
    UpdateHintText_(ctrl, mod);
    return true;
}

bool ConstantGradeHoveringState::OnMouseDownL(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    const uint32_t) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return false;

    dragState_.startX = tileX;
    dragState_.startZ = tileZ;
    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    ctrl.TransitionTo(ControlStateId::Selecting);
    return true;
}

bool ConstantGradeHoveringState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    const uint32_t mod,
    const int32_t delta) {
    const auto parameter = settings_.parameters.FindByModifiers(mod);
    if (!parameter.has_value()) return false;
    parameter->AdjustByDelta(delta);
    UpdateHintText_(ctrl, mod);
    return true;
}

bool ConstantGradeHoveringState::OnKeyDown(
    StatefulDragViewInputControl& ctrl,
    const int32_t vk,
    const uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.Close();
        return true;
    }

    UpdateHintText_(ctrl, mod);
    return false;
}

void ConstantGradeHoveringState::UpdateHintText_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers) const {
    const std::string body = std::format(
        "Drag to create constant grade path\n{}\n{}",
        settings_.ValueLabel(),
        settings_.parameters.BuildHintText(modifiers));
    ctrl.SetCursorText(
        StatefulDragViewInputControl::kPrimaryCursorSlot,
        cRZBaseString("Constant grade"),
        cRZBaseString(body.c_str()));
}

void ConstantGradeHoveringState::UpdateHoverSelection_(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z) const {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        ctrl.ClearSelections();
        renderer_.ClearHoverTile();
        return;
    }

    (void)ctrl.MarkSelected(tileX, tileZ, tileX, tileZ, cISTETerrain::eHilightColorType::Blue, true);
    renderer_.ShowHoverTile(ctrl.GetTerrain(), tileX, tileZ);
}
