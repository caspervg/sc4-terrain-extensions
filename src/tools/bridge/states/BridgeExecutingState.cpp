#include "BridgeExecutingState.hpp"

#include <algorithm>
#include <cstdlib>

#include "tools/bridge/BridgeApproachDragTool.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "tools/bridge/BridgePlacement.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
#include "utils/Logger.h"
#include "../BridgeApproachTool.hpp"
#include "controls/StatefulDragViewInputControl.hpp"

namespace {

std::optional<BridgePlacement> ResolvePlacementFromDragState(
	const BridgeDragState& dragState,
	const int widthTiles) {
	const int deltaX = dragState.currentX - dragState.startX;
	const int deltaZ = dragState.currentZ - dragState.startZ;
	if (deltaX == 0 && deltaZ == 0) {
		return std::nullopt;
	}

	const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
	const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(widthTiles);
	if (isHorizontal) {
		const int minZ = std::min(dragState.startZ, dragState.currentZ);
		const int centerZ = minZ + widthOffsets.negativeOffset;
		return ComputeBridgePlacement(
			dragState.startX,
			centerZ,
			dragState.currentX,
			centerZ);
	}

	const int minX = std::min(dragState.startX, dragState.currentX);
	const int centerX = minX + widthOffsets.negativeOffset;
	return ComputeBridgePlacement(
		centerX,
		dragState.startZ,
		centerX,
		dragState.currentZ);
}

} // namespace

BridgeExecutingState::BridgeExecutingState(BridgeToolSettings& settings, BridgeDragState& dragState)
	: settings_(settings)
	, dragState_(dragState) {}

void BridgeExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	const int deltaX = dragState_.currentX - dragState_.startX;
	const int deltaZ = dragState_.currentZ - dragState_.startZ;
	const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
	const int dragWidthTiles = isHorizontal
		? (std::abs(deltaZ) + 1)
		: (std::abs(deltaX) + 1);

	if (dragWidthTiles > settings_.widthTiles.maxValue) {
		LOG_ERROR("BridgeExecutingState::OnEnter - drag width {} exceeds max {}", dragWidthTiles, settings_.widthTiles.maxValue);
		ctrl.TransitionTo(ControlStateId::Hovering);
		return;
	}

	const int widthTiles = std::clamp(
		dragWidthTiles,
		settings_.widthTiles.minValue,
		settings_.widthTiles.maxValue);
	settings_.widthTiles.value = widthTiles;

	const auto placement = ResolvePlacementFromDragState(dragState_, widthTiles);

	if (!placement.has_value() || !placement->IsValid()) {
		LOG_ERROR("BridgeExecutingState::OnEnter - invalid bridge placement on enter!");
		ctrl.TransitionTo(ControlStateId::Hovering);
		return;
	}

	const auto height = settings_.height.value;
	const auto grade = settings_.grade.value;
	const auto width = static_cast<float>(widthTiles);

	BridgeApproachTool tool(ctrl.GetTerrain());
	tool.CreateBridgeApproaches(
		placement->bridgeStartX, placement->bridgeStartZ,
		placement->bridgeEndX, placement->bridgeEndZ,
		height, -1.0f, grade, width
	);

	ctrl.ClearSelections();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
	ctrl.TransitionTo(ControlStateId::Hovering);
}
