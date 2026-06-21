#include "ConstantGradeSelectingState.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "../ConstantGradeRenderer.hpp"
#include "../ConstantGradeSettings.hpp"
#include "utils/Logger.h"

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
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return true;
    if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) return true;

    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::OnMouseUpL(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    const uint32_t) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        dragState_.currentX = tileX;
        dragState_.currentZ = tileZ;
    }

    if (!BuildRequest_().has_value()) {
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
    //ctrl.TransitionTo(ControlStateId::Hovering);
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

    return RebuildPreview_(ctrl, mod);
}

bool ConstantGradeSelectingState::RebuildPreview_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers) {
    const auto request = BuildRequest_();
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
        "Release to apply\nlength {} tiles | width {} tiles | start {:.1f}m | end {:.1f}m\n{}",
        preview->pathLengthTiles,
        preview->effectiveWidthTiles,
        preview->startHeight,
        preview->endHeight,
        settings_.parameters.BuildHintText(modifiers));
    ctrl.SetCursorText(
        StatefulDragViewInputControl::kPrimaryCursorSlot,
        cRZBaseString("Constant grade"),
        cRZBaseString(body.c_str()));
    return true;
}

std::optional<ConstantGradeRequest> ConstantGradeSelectingState::BuildRequest_() const {
    const int deltaX = dragState_.currentX - dragState_.startX;
    const int deltaZ = dragState_.currentZ - dragState_.startZ;
    if (deltaX == 0 && deltaZ == 0) return std::nullopt;

    const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
    const int dragWidthTiles = isHorizontal ? (std::abs(deltaZ) + 1) : (std::abs(deltaX) + 1);
    const int effectiveWidthTiles = std::max(1, dragWidthTiles);
    const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(effectiveWidthTiles);

    if (isHorizontal) {
        const int minZ = std::min(dragState_.startZ, dragState_.currentZ);
        const int centerZ = minZ + widthOffsets.negativeOffset;
        return ConstantGradeRequest{
            .startTileX = dragState_.startX,
            .startTileZ = centerZ,
            .endTileX = dragState_.currentX,
            .endTileZ = centerZ,
            .widthTiles = static_cast<float>(effectiveWidthTiles),
            .gradePercent = settings_.gradePercent.value,
            .sideSmoothing = settings_.IsSideSmoothingEnabled(),
        };
    }

    const int minX = std::min(dragState_.startX, dragState_.currentX);
    const int centerX = minX + widthOffsets.negativeOffset;
    return ConstantGradeRequest{
        .startTileX = centerX,
        .startTileZ = dragState_.startZ,
        .endTileX = centerX,
        .endTileZ = dragState_.currentZ,
        .widthTiles = static_cast<float>(effectiveWidthTiles),
        .gradePercent = settings_.gradePercent.value,
        .sideSmoothing = settings_.IsSideSmoothingEnabled(),
    };
}
