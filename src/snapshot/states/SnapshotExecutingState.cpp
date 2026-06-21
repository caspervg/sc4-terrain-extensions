#include "SnapshotExecutingState.hpp"

#include <algorithm>

#include "controls/StatefulDragViewInputControl.hpp"
#include "snapshot/SnapshotDragState.hpp"
#include "snapshot/SnapshotManager.hpp"
#include "snapshot/SnapshotPreviewRenderer.hpp"
#include "utils/Logger.h"

SnapshotExecutingState::SnapshotExecutingState(
	SnapshotManager& mgr,
	SnapshotPreviewRenderer& renderer,
	SnapshotDragState& dragState)
	: mgr_(mgr)
	, renderer_(renderer)
	, dragState_(dragState)
{
}

void SnapshotExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
	// Compute region in vertex coords (tiles map to vertices: tile N covers vertices N..N+1)
	const int minTileX = std::min(dragState_.startX, dragState_.currentX);
	const int minTileZ = std::min(dragState_.startZ, dragState_.currentZ);
	const int maxTileX = std::max(dragState_.startX, dragState_.currentX);
	const int maxTileZ = std::max(dragState_.startZ, dragState_.currentZ);

	// Vertex range: include all corners of selected tiles
	const int minVX = minTileX;
	const int minVZ = minTileZ;
	const int maxVX = maxTileX + 1;
	const int maxVZ = maxTileZ + 1;

	mgr_.RestoreRegion(dragState_.restoreIndex, ctrl.GetTerrain(),
	                    minVX, minVZ, maxVX, maxVZ);

	LOG_INFO("SnapshotExecutingState: restored region [{},{} - {},{}]",
		minVX, minVZ, maxVX, maxVZ);

	// Clean up: disable preview and deactivate
	mgr_.SetPreviewIndex(-1);
	renderer_.ClearAll();

	ctrl.ClearSelections();
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
	ctrl.Close();
}
