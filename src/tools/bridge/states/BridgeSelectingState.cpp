#include "BridgeSelectingState.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "tools/bridge/BridgePlacement.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
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

	const std::string body = std::format(
		"Drag to set bridge span and width | Right-click to cancel\n{}",
		settings_.parameters.BuildHintText(0));
	const cRZBaseString title("Bridge builder");
	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, body.c_str());
}

bool BridgeSelectingState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return true;

	if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) return true;

	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;
	RebuildPreview_(ctrl, mod);
	return true;
}

bool BridgeSelectingState::OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	//ctrl.TransitionTo(ControlStateId::Hovering);
	return false;
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

void BridgeSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl, const uint32_t modifiers) {
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
	ctrl.ClearSelections();

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
			isValid,
			settings_.sideMode
		);
	} else {
		renderer_.ClearAll();
	}

	renderer_.ShowHoverTile(ctrl.GetTerrain(), dragState_.currentX, dragState_.currentZ);

	const std::string statusText = isValid
		? "Release to place | Right-click to cancel"
		: (widthTooWide
			? "Too wide - max 10 tiles | Right-click to cancel"
			: "Too short - drag further | Right-click to cancel");
	const std::string body = std::format("{}\n{}", statusText, settings_.parameters.BuildHintText(modifiers));

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
	RebuildPreview_(ctrl, mod);
	return true;
}

bool BridgeSelectingState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
	if (vk == 0x1B) { // VK_ESCAPE
		ctrl.TransitionTo(ControlStateId::Inactive);
		return true;
	}

	RebuildPreview_(ctrl, mod);
	return false;
}
