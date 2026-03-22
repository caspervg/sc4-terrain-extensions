#include "ConstantGradeExecutingState.hpp"

#include <algorithm>
#include <cmath>

#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "../ConstantGradeOperation.hpp"
#include "../ConstantGradeSettings.hpp"
#include "utils/Logger.h"

ConstantGradeExecutingState::ConstantGradeExecutingState(
    ConstantGradeSettings& settings,
    ConstantGradeOperation& operation,
    ConstantGradeDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , dragState_(dragState) {
}

void ConstantGradeExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    const int deltaX = dragState_.currentX - dragState_.startX;
    const int deltaZ = dragState_.currentZ - dragState_.startZ;
    if (deltaX == 0 && deltaZ == 0) {
        ctrl.TransitionTo(ControlStateId::Hovering);
        return;
    }

    const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
    const int dragWidthTiles = isHorizontal ? (std::abs(deltaZ) + 1) : (std::abs(deltaX) + 1);
    const int effectiveWidthTiles = std::max(1, dragWidthTiles);
    const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(effectiveWidthTiles);

    ConstantGradeRequest request{};
    if (isHorizontal) {
        const int minZ = std::min(dragState_.startZ, dragState_.currentZ);
        const int centerZ = minZ + widthOffsets.negativeOffset;
        request.startTileX = dragState_.startX;
        request.startTileZ = centerZ;
        request.endTileX = dragState_.currentX;
        request.endTileZ = centerZ;
    } else {
        const int minX = std::min(dragState_.startX, dragState_.currentX);
        const int centerX = minX + widthOffsets.negativeOffset;
        request.startTileX = centerX;
        request.startTileZ = dragState_.startZ;
        request.endTileX = centerX;
        request.endTileZ = dragState_.currentZ;
    }
    request.widthTiles = static_cast<float>(effectiveWidthTiles);
    request.gradePercent = settings_.gradePercent.value;

    if (!operation_.Apply(request)) {
        LOG_WARN("ConstantGradeExecutingState: grade apply failed");
    }

    ctrl.TransitionTo(ControlStateId::Hovering);
}
