#include "SnapshotSelectingState.hpp"

#include <algorithm>
#include <format>

#include "cRZBaseString.h"
#include "cISTETerrain.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "snapshot/SnapshotDragState.hpp"
#include "snapshot/SnapshotManager.hpp"
#include "snapshot/SnapshotPreviewRenderer.hpp"
#include "utils/Logger.h"

SnapshotSelectingState::SnapshotSelectingState(
	SnapshotManager& mgr,
	SnapshotPreviewRenderer& renderer,
	SnapshotDragState& dragState)
	: mgr_(mgr)
	, renderer_(renderer)
	, dragState_(dragState)
{
}

void SnapshotSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.BeginCapture();
	UpdateSelection_(ctrl);
}

void SnapshotSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
	ctrl.EndCapture();
	ctrl.ClearSelections();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
}

bool SnapshotSelectingState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return true;

	if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) return true;

	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;
	UpdateSelection_(ctrl);
	return true;
}

bool SnapshotSelectingState::OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;

	// Need at least a 1-tile selection
	if (dragState_.startX == dragState_.currentX && dragState_.startZ == dragState_.currentZ) {
		ctrl.TransitionTo(ControlStateId::Hovering);
		return true;
	}

	ctrl.TransitionTo(ControlStateId::Executing);
	return true;
}

bool SnapshotSelectingState::OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	// Cancel
	ctrl.TransitionTo(ControlStateId::Hovering);
	return true;
}

bool SnapshotSelectingState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
	if (vk == 0x1B) { // VK_ESCAPE
		ctrl.TransitionTo(ControlStateId::Inactive);
		return true;
	}
	return false;
}

void SnapshotSelectingState::UpdateSelection_(StatefulDragViewInputControl& ctrl) {
	const int minX = std::min(dragState_.startX, dragState_.currentX);
	const int minZ = std::min(dragState_.startZ, dragState_.currentZ);
	const int maxX = std::max(dragState_.startX, dragState_.currentX);
	const int maxZ = std::max(dragState_.startZ, dragState_.currentZ);

	ctrl.MarkSelected(minX, minZ, maxX, maxZ,
	                   cISTETerrain::eHilightColorType::Blue, true);

	const int width = maxX - minX + 1;
	const int height = maxZ - minZ + 1;

	const std::string body = std::format(
		"Selection: {}x{} tiles\nRelease to restore | Right-click to cancel",
		width, height);

	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot,
	                    "Snapshot Restore", body);
}
