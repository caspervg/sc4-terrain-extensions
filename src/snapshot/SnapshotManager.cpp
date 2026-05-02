#include "SnapshotManager.hpp"
#include "cISTETerrain.h"
#include "SC4Rect.h"
#include "utils/Logger.h"

bool SnapshotManager::Capture(cISTETerrain* terrain, const std::string& name, const std::string& desc) {
	if (!terrain) return false;

	if (IsFull()) {
		LOG_DEBUG("SnapshotManager: buffer full, evicting oldest snapshot ('{}')", snapshots_[0].name);
		Remove(0);
	}

	const uint32_t cx = terrain->CellCountX() + 1;
	const uint32_t cz = terrain->CellCountZ() + 1;

	TerrainSnapshot snap;
	snap.name = name;
	snap.description = desc;
	snap.timestamp = std::chrono::system_clock::now();
	snap.vertexCountX = cx;
	snap.vertexCountZ = cz;
	snap.heights.resize(static_cast<size_t>(cx) * cz);

	for (uint32_t z = 0; z < cz; ++z) {
		for (uint32_t x = 0; x < cx; ++x) {
			snap.heights[static_cast<size_t>(z) * cx + x] =
				terrain->GetAltitudeAtVertex(static_cast<int>(x), static_cast<int>(z));
		}
	}

	snapshots_.push_back(std::move(snap));
	LOG_INFO("SnapshotManager: captured '{}' ({}x{} vertices)", name, cx, cz);
	return true;
}

void SnapshotManager::RestoreFull(int index, cISTETerrain* terrain) {
	const auto* snap = Get(index);
	if (!snap || !terrain) return;

	for (uint32_t z = 0; z < snap->vertexCountZ; ++z) {
		for (uint32_t x = 0; x < snap->vertexCountX; ++x) {
			terrain->SetAltitudeAtVertex(
				static_cast<int>(x), static_cast<int>(z),
				snap->GetHeight(static_cast<int>(x), static_cast<int>(z)));
		}
	}

	// Rect uses cell coordinates (vertexCount - 1 = cellCount)
	SC4Rect<int32_t> rect{0, 0,
		static_cast<int32_t>(snap->vertexCountX - 1),
		static_cast<int32_t>(snap->vertexCountZ - 1)};
	terrain->RedisplayTerrain(true, true, rect, 0);

	LOG_INFO("SnapshotManager: restored full terrain from '{}'", snap->name);
}

void SnapshotManager::RestoreRegion(int index, cISTETerrain* terrain,
                                     int minX, int minZ, int maxX, int maxZ) {
	const auto* snap = Get(index);
	if (!snap || !terrain) return;

	// Clamp to snapshot bounds (vertex coords)
	minX = std::max(minX, 0);
	minZ = std::max(minZ, 0);
	maxX = std::min(maxX, static_cast<int>(snap->vertexCountX - 1));
	maxZ = std::min(maxZ, static_cast<int>(snap->vertexCountZ - 1));

	for (int z = minZ; z <= maxZ; ++z) {
		for (int x = minX; x <= maxX; ++x) {
			terrain->SetAltitudeAtVertex(x, z, snap->GetHeight(x, z));
		}
	}

	SC4Rect<int32_t> rect{minX, minZ, maxX + 1, maxZ + 1};
	terrain->RedisplayTerrain(true, true, rect, 0);

	LOG_INFO("SnapshotManager: restored region [{},{} - {},{}] from '{}'",
		minX, minZ, maxX, maxZ, snap->name);
}

void SnapshotManager::Remove(int index) {
	if (index < 0 || static_cast<size_t>(index) >= snapshots_.size()) return;

	const std::string name = snapshots_[index].name;
	snapshots_.erase(snapshots_.begin() + index);

	// Adjust preview index
	if (previewIndex_ == index) {
		previewIndex_ = -1;
	} else if (previewIndex_ > index) {
		--previewIndex_;
	}

	LOG_INFO("SnapshotManager: removed '{}'", name);
}

void SnapshotManager::Clear() {
	snapshots_.clear();
	previewIndex_ = -1;
}

const TerrainSnapshot* SnapshotManager::Get(int index) const {
	if (index < 0 || static_cast<size_t>(index) >= snapshots_.size()) return nullptr;
	return &snapshots_[index];
}
