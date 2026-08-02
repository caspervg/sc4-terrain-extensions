#include "ConstantGradeSelectingState.hpp"

#include <format>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "../ConstantGradeRenderer.hpp"
#include "../ConstantGradeRequestBuilder.hpp"
#include "../ConstantGradeSettings.hpp"
#include "tools/ToolParameter.hpp"
#include "utils/Logger.h"

namespace {
constexpr int32_t kToggleShapeKey = 0x44; // D
constexpr int32_t kShiftKey = 0x10;
}

ConstantGradeSelectingState::ConstantGradeSelectingState(
    ConstantGradeSettings& settings,
    ConstantGradeOperation& operation,
    ConstantGradeRenderer& renderer,
    ConstantGradeDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void ConstantGradeSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    if (!ctrl.BeginCapture()) {
        LOG_WARN("ConstantGradeSelectingState: failed to acquire mouse capture; aborting selection");
        ctrl.TransitionTo(ControlStateId::Hovering);
        return;
    }
    RebuildPreview_(ctrl, 0);
}

void ConstantGradeSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.EndCapture();
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
    renderer_.ClearAll();
}

bool ConstantGradeSelectingState::OnMouseMove(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    const uint32_t mod) {
    const bool snapChanged = SyncSnapAngle_(mod);

    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return true;
    if (tileX == dragState_.currentX && tileZ == dragState_.currentZ && !snapChanged) return true;

    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::OnMouseUpL(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    const uint32_t mod) {
    SyncSnapAngle_(mod);

    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        dragState_.currentX = tileX;
        dragState_.currentZ = tileZ;
    }

    const auto request = BuildConstantGradeRequest(dragState_, settings_);
    if (!request.has_value() || !operation_.BuildPreview(*request).has_value()) {
        ctrl.TransitionTo(ControlStateId::Hovering);
        return true;
    }

    ctrl.TransitionTo(ControlStateId::Executing);
    return true;
}

bool ConstantGradeSelectingState::OnMouseDownR(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t) {
    ctrl.TransitionTo(ControlStateId::Hovering);
    return false;
}

bool ConstantGradeSelectingState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    const uint32_t mod,
    const int32_t delta) {
    const auto parameter = settings_.parameters.FindByModifiers(mod);
    if (!parameter.has_value()) return false;
    parameter->AdjustByDelta(delta);
    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::OnKeyDown(
    StatefulDragViewInputControl& ctrl,
    const int32_t vk,
    const uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.Close();
        return true;
    }

    if (vk == kToggleShapeKey) {
        settings_.CycleShape(1);
        return RebuildPreview_(ctrl, mod);
    }

    if (vk == kShiftKey) {
        dragState_.snapAngle = true;
        return RebuildPreview_(ctrl, mod);
    }

    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::OnKeyUp(
    StatefulDragViewInputControl& ctrl,
    const int32_t vk,
    const uint32_t mod) {
    if (vk != kShiftKey) return false;

    dragState_.snapAngle = false;
    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::SyncSnapAngle_(const uint32_t modifiers) {
    const bool snap = (modifiers & ModifierCombo::kShift) != 0;
    if (dragState_.snapAngle == snap) return false;

    dragState_.snapAngle = snap;
    return true;
}

bool ConstantGradeSelectingState::RebuildPreview_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers) {
    const auto request = BuildConstantGradeRequest(dragState_, settings_);
    if (!request.has_value()) {
        renderer_.ClearAll();
        ctrl.ClearSelections();
        return false;
    }

    const auto preview = operation_.BuildPreview(*request);
    if (!preview.has_value()) {
        renderer_.ClearAll();
        ctrl.ClearSelections();
        return false;
    }

    ctrl.ClearSelections();
    renderer_.Update(ctrl.GetTerrain(), *preview);
    renderer_.ShowHoverTile(ctrl.GetTerrain(), dragState_.currentX, dragState_.currentZ);

    const std::string body = std::format(
        "Release to apply\nlength {} tiles | width {} tiles | start {:.1f}m | end {:.1f}m\n"
        "dir {}{}\n{}\nD: toggle rectangle/line{}",
        preview->pathLengthTiles,
        preview->requestedWidthTiles,
        preview->startHeight,
        preview->endHeight,
        DescribeConstantGradeDirection(*request),
        dragState_.snapAngle ? " [snapped]" : "",
        settings_.parameters.BuildHintText(modifiers),
        settings_.IsLineMode() ? "\nHold Shift: snap to 45 deg" : "");
    ctrl.SetCursorText(
        StatefulDragViewInputControl::kPrimaryCursorSlot,
        cRZBaseString("Constant grade"),
        cRZBaseString(body.c_str()));
    return true;
}
