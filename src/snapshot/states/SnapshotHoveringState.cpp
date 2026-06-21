#include "SnapshotHoveringState.hpp"

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "snapshot/SnapshotDragState.hpp"
#include "snapshot/SnapshotManager.hpp"
#include "snapshot/SnapshotPreviewRenderer.hpp"
#include "utils/Logger.h"

SnapshotHoveringState::SnapshotHoveringState(
	SnapshotManager& mgr,
	SnapshotPreviewRenderer& renderer,
	SnapshotDragState& dragState)
	: mgr_(mgr)
	, renderer_(renderer)
	, dragState_(dragState)
{
}

void SnapshotHoveringState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearSelections();

	// Auto-enable wireframe preview for the target snapshot
	const auto* snap = mgr_.GetById(dragState_.restoreId);
	if (snap) {
		mgr_.SetPreviewIndex(mgr_.IndexOfId(dragState_.restoreId));
		renderer_.Rebuild(*snap, ctrl.GetTerrain());
	}

	const cRZBaseString body("Drag to select restore region | ESC to cancel");
	const cRZBaseString title("Snapshot Restore");
	ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, body);

	LOG_DEBUG("SnapshotHoveringState::OnEnter (snapshot id={})", dragState_.restoreId);
}

void SnapshotHoveringState::OnExit(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
}

bool SnapshotHoveringState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	return true;
}

bool SnapshotHoveringState::OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
	int32_t tileX, tileZ;
	if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) return false;

	dragState_.startX = tileX;
	dragState_.startZ = tileZ;
	dragState_.currentX = tileX;
	dragState_.currentZ = tileZ;

	ctrl.TransitionTo(ControlStateId::Selecting);
	return true;
}

bool SnapshotHoveringState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
	if (vk == 0x1B) { // VK_ESCAPE
		ctrl.Close();
		return true;
	}
	return false;
}
