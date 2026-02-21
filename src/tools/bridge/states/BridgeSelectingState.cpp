#include "BridgeSelectingState.hpp"

#include <algorithm>
#include <cstdlib>

#include "cRZBaseString.h"
#include "cISTETerrain.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "tools/bridge/BridgePlacement.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
#include "utils/Logger.h"
#include "viz/BridgeApproachRenderer.hpp"

namespace {

float SampleTileHeight(void* context, const int tileX, const int tileZ) {
	auto* terrain = static_cast<cISTETerrain*>(context);
	return BridgeApproachGeometry::SampleTileAverageHeight(terrain, tileX, tileZ);
}

std::optional<BridgeApproachGeometry::ApproachParams> BuildPreviewGeometry(
	cISTETerrain* terrain,
	const BridgePlacement& placement,
	const BridgeToolSettings& settings) {
	if (!terrain) return std::nullopt;

	const int maxTileX = static_cast<int>(terrain->CellCountX()) - 1;
	const int maxTileZ = static_cast<int>(terrain->CellCountZ()) - 1;

	return BridgeApproachGeometry::BuildApproachParams(
		placement,
		settings.height.value,
		settings.grade.value,
		static_cast<float>(settings.widthTiles.value),
		maxTileX,
		maxTileZ,
		&SampleTileHeight,
		terrain
	);
}

} // namespace

BridgeSelectingState::BridgeSelectingState(BridgeToolSettings& settings,
                                           BridgeApproachRenderer& renderer,
                                           BridgeDragState& dragState)
	: settings_(settings)
	, renderer_(renderer)
	, dragState_(dragState) {}

void BridgeSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.BeginCapture();
	settings_.widthTiles.value = settings_.widthTiles.minValue;

	const auto sel = ctrl.MarkSelected(dragState_.startX, dragState_.startZ,
	                  dragState_.currentX, dragState_.currentZ,
	                  cISTETerrain::eHilightColorType::Blue,
	                  true
	);

	if (!sel) {
		LOG_WARN("BridgeSelectingState::OnEnter - failed to mark selection");
	}

	const cRZBaseString body("Drag to set bridge span and width | Right-click to cancel");
	const cRZBaseString title("Bridge builder");
	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, body);
}

bool BridgeSelectingState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return true;

	if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) return true;

	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;
	RebuildPreview_(ctrl);
	return true;
}

bool BridgeSelectingState::OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	ctrl.TransitionTo(ControlStateId::Hovering);
	return true;
}

bool BridgeSelectingState::OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;

	const auto placement = ResolvePlacementFromDrag_();
	if (!placement.has_value() || !placement->IsValid()) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	ctrl.TransitionTo(ControlStateId::Executing);
	return true;
}

void BridgeSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl) {
	const int deltaX = dragState_.currentX - dragState_.startX;
	const int deltaZ = dragState_.currentZ - dragState_.startZ;
	const bool hasDrag = (deltaX != 0 || deltaZ != 0);
	const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
	const int dragWidthTiles = isHorizontal
		? (std::abs(deltaZ) + 1)
		: (std::abs(deltaX) + 1);
	const bool widthTooWide = hasDrag && (dragWidthTiles > settings_.widthTiles.maxValue);

	const auto placement = ResolvePlacementFromDrag_();
	const bool isValid = placement.has_value() && placement->IsValid();

	bool sel = false;
	if (placement.has_value()) {
		int minTileX = std::min(placement->bridgeStartX, placement->bridgeEndX);
		int maxTileX = std::max(placement->bridgeStartX, placement->bridgeEndX);
		int minTileZ = std::min(placement->bridgeStartZ, placement->bridgeEndZ);
		int maxTileZ = std::max(placement->bridgeStartZ, placement->bridgeEndZ);

		const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(
			settings_.widthTiles.value);
		if (placement->isHorizontal) {
			minTileZ -= widthOffsets.negativeOffset;
			maxTileZ += widthOffsets.positiveOffset;
		} else {
			minTileX -= widthOffsets.negativeOffset;
			maxTileX += widthOffsets.positiveOffset;
		}

		sel = ctrl.MarkSelected(
			minTileX,
			minTileZ,
			maxTileX,
			maxTileZ,
			isValid ? cISTETerrain::eHilightColorType::Green : cISTETerrain::eHilightColorType::Red,
			true
		);
	} else {
		sel = ctrl.MarkSelected(
			dragState_.startX,
			dragState_.startZ,
			dragState_.currentX,
			dragState_.currentZ,
			cISTETerrain::eHilightColorType::Red,
			true
		);
	}

	if (!sel) {
		LOG_WARN("BridgeSelectingState::RebuildPreview_ - failed to mark selection");
	}

	if (placement.has_value()) {
		const auto geometry = BuildPreviewGeometry(ctrl.GetTerrain(), *placement, settings_);
		if (!geometry.has_value()) {
			renderer_.ClearAll();
			return;
		}

		renderer_.Update(
			ctrl.GetTerrain(),
			*geometry,
			settings_.showHeightMarkers,
			isValid
		);
	} else {
		renderer_.ClearAll();
	}

	const std::string statusText = isValid
		? "Release to place | Right-click to cancel"
		: (widthTooWide
			? "Too wide - max 10 tiles | Right-click to cancel"
			: "Too short - drag further | Right-click to cancel");
	const std::string body = std::format("{}\n{}", statusText, settings_.parameters.BuildHintText(0));

	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, "Bridge builder", body);
}

std::optional<BridgePlacement> BridgeSelectingState::ResolvePlacementFromDrag_() const {
	const int deltaX = dragState_.currentX - dragState_.startX;
	const int deltaZ = dragState_.currentZ - dragState_.startZ;

	if (deltaX == 0 && deltaZ == 0) {
		return std::nullopt;
	}

	const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
	const int dragWidthTiles = isHorizontal
		? (std::abs(deltaZ) + 1)
		: (std::abs(deltaX) + 1);

	if (dragWidthTiles > settings_.widthTiles.maxValue) {
		settings_.widthTiles.value = settings_.widthTiles.maxValue;
		return std::nullopt;
	}

	settings_.widthTiles.value = std::max(
		dragWidthTiles,
		settings_.widthTiles.minValue);

	const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(
		settings_.widthTiles.value);
	if (isHorizontal) {
		const int minZ = std::min(dragState_.startZ, dragState_.currentZ);
		const int centerZ = minZ + widthOffsets.negativeOffset;
		return ComputeBridgePlacement(
			dragState_.startX,
			centerZ,
			dragState_.currentX,
			centerZ);
	}

	const int minX = std::min(dragState_.startX, dragState_.currentX);
	const int centerX = minX + widthOffsets.negativeOffset;
	return ComputeBridgePlacement(
		centerX,
		dragState_.startZ,
		centerX,
		dragState_.currentZ);
}

void BridgeSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
	ctrl.EndCapture();
	ctrl.ClearSelections();
	renderer_.ClearAll();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
}

bool BridgeSelectingState::OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) {
	const auto param = settings_.parameters.FindByModifiers(mod);
	if (!param.has_value()) return false;
	param->AdjustByDelta(delta);
	RebuildPreview_(ctrl);
	return true;
}

bool BridgeSelectingState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
	if (vk == 0x1B) { // VK_ESCAPE
		ctrl.TransitionTo(ControlStateId::Inactive);
		return true;
	}

	RebuildPreview_(ctrl);
	return false;
}
