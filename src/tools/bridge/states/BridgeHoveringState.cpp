#include "BridgeHoveringState.hpp"

#include <utils/Logger.h>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
#include "tools/bridge/IBridgeDragContext.hpp"
#include "viz/BridgeApproachRenderer.hpp"

BridgeHoveringState::BridgeHoveringState(
	BridgeToolSettings& settings,
	BridgeApproachRenderer& renderer,
	IBridgeDragContext& context)
	: settings_(settings)
	, renderer_(renderer)
	, context_(context)
{
}

void BridgeHoveringState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearSelections();
	renderer_.ClearAll();

	UpdateHintText_(ctrl, 0);
	LOG_DEBUG("BridgeHoveringState::OnEnter");
}

void BridgeHoveringState::OnExit(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearCursorText(StatefulDragViewInputControl::kSecondaryTextSlot);
}

bool BridgeHoveringState::OnMouseMove(StatefulDragViewInputControl& ctrl, const int32_t x, const int32_t z, const uint32_t mod) {
	UpdateHintText_(ctrl, mod);
	return true;
}

bool BridgeHoveringState::OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return false;

	context_.SetDragStart(tileX, tileZ);
	context_.SetDragCurrent(tileX, tileZ);

	ctrl.TransitionTo(ControlStateId::Selecting);
	return true;
}

bool BridgeHoveringState::OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) {
	auto param = settings_.parameters.FindByModifiers(mod);
	if (!param.has_value()) return false;
	param->AdjustByDelta(delta);
	UpdateHintText_(ctrl, mod);
	return true;
}

bool BridgeHoveringState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
	if (vk == 0x1B) { // VK_ESCAPE
		ctrl.TransitionTo(ControlStateId::Inactive);
		return true;
	}

	UpdateHintText_(ctrl, mod);
	return false;
}

void BridgeHoveringState::UpdateHintText_(StatefulDragViewInputControl& ctrl, uint32_t modifiers) const {
	const std::string hint = settings_.parameters.BuildHintText(modifiers);
	const cRZBaseString body(hint.c_str());
	const cRZBaseString title("Bridge approach tool");
	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryTextSlot, body, title);
}
