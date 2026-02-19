#include "BridgeSelectingState.hpp"

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
#include "utils/Logger.h"
#include "viz/BridgeApproachRenderer.hpp"

BridgeSelectingState::BridgeSelectingState(BridgeToolSettings& settings,
                                           BridgeApproachRenderer& renderer,
                                           IBridgeDragContext& context)
	: settings_(settings)
	, renderer_(renderer)
	, context_(context) {}

void BridgeSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.BeginCapture();

	const auto sel = ctrl.MarkSelected(context_.GetDragStartX(), context_.GetDragStartZ(),
	                  context_.GetDragCurrentX(), context_.GetDragCurrentZ(),
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

	if (tileX == context_.GetDragCurrentX() && tileZ == context_.GetDragCurrentZ()) return true;

	context_.SetDragCurrent(tileX, tileZ);
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

	context_.SetDragCurrent(tileX, tileZ);

	const auto placement = context_.ComputePlacement();
	if (!placement.has_value() || !placement->IsValid()) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	ctrl.TransitionTo(ControlStateId::Executing);
	return true;
}

void BridgeSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl) const {
	const auto placement = context_.ComputePlacement();
	const bool isValid = placement.has_value() && placement->IsValid();

	const auto sel = ctrl.MarkSelected(
		context_.GetDragStartX(), context_.GetDragStartZ(),
		context_.GetDragCurrentX(), context_.GetDragCurrentZ(),
		isValid ? cISTETerrain::eHilightColorType::Green : cISTETerrain::eHilightColorType::Red,
		true
	);

	if (!sel) {
		LOG_WARN("BridgeSelectingState::RebuildPreview_ - failed to mark selection");
	}

	if (placement.has_value()) {
		renderer_.Update(
			ctrl.GetTerrain(),
			placement->bridgeStartX,
			placement->bridgeStartZ,
			placement->bridgeEndX,
			placement->bridgeEndZ,
			settings_,
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
