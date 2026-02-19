#include "BridgeExecutingState.hpp"

#include "tools/bridge/BridgeApproachDragTool.hpp"
#include "tools/bridge/BridgeToolSettings.hpp"
#include "tools/bridge/IBridgeDragContext.hpp"
#include "utils/Logger.h"
#include "../BridgeApproachTool.hpp"
#include "controls/StatefulDragViewInputControl.hpp"

BridgeExecutingState::BridgeExecutingState(BridgeToolSettings& settings, IBridgeDragContext& context)
	: settings_(settings)
	, context_(context) {}

void BridgeExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	const auto placement = context_.ComputePlacement();

	if (!placement.has_value() || !placement->IsValid()) {
		LOG_ERROR("BridgeExecutingState::OnEnter - invalid bridge placement on enter!");
		ctrl.TransitionTo(ControlStateId::Hovering);
		return;
	}

	const auto height = settings_.height.value;
	const auto grade = settings_.grade.value;
	const auto width = static_cast<float>(settings_.widthTiles.value);

	BridgeApproachTool tool(ctrl.GetTerrain());
	tool.CreateBridgeApproaches(
		placement->bridgeStartX, placement->bridgeStartZ,
		placement->bridgeEndX, placement->bridgeEndZ,
		height, -1.0f, grade, width
	);

	ctrl.ClearSelections();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryTextSlot);
	ctrl.ClearCursorText(StatefulDragViewInputControl::kSecondaryTextSlot);
	ctrl.TransitionTo(ControlStateId::Hovering);
}
