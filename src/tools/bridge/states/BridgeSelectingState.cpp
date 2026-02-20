#include "BridgeSelectingState.hpp"

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

	const auto sel = ctrl.MarkSelected(dragState_.startX, dragState_.startZ,
	                  dragState_.currentX, dragState_.currentZ,
	                  cISTETerrain::eHilightColorType::Blue,
	                  true
	);

	if (!sel) {
		LOG_WARN("BridgeSelectingState::OnEnter - failed to mark selection");
	}

	const cRZBaseString body("Drag to set bridge span | Right-click to cancel");
	const cRZBaseString title("Bridge approach tool");
	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryTextSlot, body, title);
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

	const auto placement = ComputeBridgePlacement(
		dragState_.startX,
		dragState_.startZ,
		dragState_.currentX,
		dragState_.currentZ
	);
	if (!placement.has_value() || !placement->IsValid()) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	ctrl.TransitionTo(ControlStateId::Executing);
	return true;
}

void BridgeSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl) const {
	const auto placement = ComputeBridgePlacement(
		dragState_.startX,
		dragState_.startZ,
		dragState_.currentX,
		dragState_.currentZ
	);
	const bool isValid = placement.has_value() && placement->IsValid();

	const auto sel = ctrl.MarkSelected(
		dragState_.startX, dragState_.startZ,
		dragState_.currentX, dragState_.currentZ,
		isValid ? cISTETerrain::eHilightColorType::Green : cISTETerrain::eHilightColorType::Red,
		true
	);

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

	const std::string statusText =  isValid
		? "Release to place | Right-click to cancel"
		: "Too short — drag further | Right-click to cancel";

	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryTextSlot, statusText, "Bridge approach tool");
	ctrl.SetCursorText(StatefulDragViewInputControl::kSecondaryTextSlot, settings_.parameters.BuildHintText(0).c_str(), "");
}

void BridgeSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
	ctrl.EndCapture();
	ctrl.ClearSelections();
	renderer_.ClearAll();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryTextSlot);
	ctrl.ClearCursorText(StatefulDragViewInputControl::kSecondaryTextSlot);
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
