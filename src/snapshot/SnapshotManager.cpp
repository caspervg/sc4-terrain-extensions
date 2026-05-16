#include "SnapshotManager.hpp"
#include "cISTETerrain.h"
#include "SC4Rect.h"
#include "utils/Logger.h"

#include <cmath>

namespace {
constexpr float kSnapshotHeightEpsilon = 0.001f;

bool HeightsDiffer(float a, float b) {
	return std::abs(a - b) > kSnapshotHeightEpsilon;
}

SC4Rect<int32_t> CellRectForChangedVertices(
	cISTETerrain* terrain,
	int minVertexX,
	int minVertexZ,
	int maxVertexX,
	int maxVertexZ)
{
	const auto maxCellX = static_cast<int32_t>(terrain->CellCountX());
	const auto maxCellZ = static_cast<int32_t>(terrain->CellCountZ());

	return {
		std::clamp<int32_t>(minVertexX - 1, 0, maxCellX),
		std::clamp<int32_t>(minVertexZ - 1, 0, maxCellZ),
		std::clamp<int32_t>(maxVertexX + 1, 0, maxCellX),
		std::clamp<int32_t>(maxVertexZ + 1, 0, maxCellZ)
	};
}
}

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

	bool changed = false;
	int minChangedX = static_cast<int>(snap->vertexCountX);
	int minChangedZ = static_cast<int>(snap->vertexCountZ);
	int maxChangedX = -1;
	int maxChangedZ = -1;

	for (uint32_t z = 0; z < snap->vertexCountZ; ++z) {
		for (uint32_t x = 0; x < snap->vertexCountX; ++x) {
			const auto xi = static_cast<int>(x);
			const auto zi = static_cast<int>(z);
			const float height = snap->GetHeight(xi, zi);
			if (!HeightsDiffer(terrain->GetAltitudeAtVertex(xi, zi), height)) {
				continue;
			}

			terrain->SetAltitudeAtVertex(
				xi,
				zi,
				height);

			changed = true;
			minChangedX = std::min(minChangedX, xi);
			minChangedZ = std::min(minChangedZ, zi);
			maxChangedX = std::max(maxChangedX, xi);
			maxChangedZ = std::max(maxChangedZ, zi);
		}
	}

	if (!changed) {
		LOG_INFO("SnapshotManager: full restore from '{}' skipped; terrain already matched", snap->name);
		return;
	}

	const SC4Rect<int32_t> rect = CellRectForChangedVertices(
		terrain,
		minChangedX,
		minChangedZ,
		maxChangedX,
		maxChangedZ);
	terrain->RedisplayTerrain(true, true, rect, 0);

	LOG_INFO(
		"SnapshotManager: restored full terrain from '{}' with dirty rect [{},{} - {},{}]",
		snap->name,
		rect.topLeftX, rect.topLeftY, rect.bottomRightX, rect.bottomRightY);
}

void SnapshotManager::RestoreRegion(int index, cISTETerrain* terrain,
                                     int minX, int minZ, int maxX, int maxZ) {
	const auto* snap = Get(index);
	if (!snap || !terrain) return;

	// Clamp to snapshot bounds (vertex coords).
	minX = std::max(minX, 0);
	minZ = std::max(minZ, 0);
	maxX = std::min(maxX, static_cast<int>(snap->vertexCountX - 1));
	maxZ = std::min(maxZ, static_cast<int>(snap->vertexCountZ - 1));

	bool changed = false;
	int minChangedX = maxX;
	int minChangedZ = maxZ;
	int maxChangedX = minX;
	int maxChangedZ = minZ;

	for (int z = minZ; z <= maxZ; ++z) {
		for (int x = minX; x <= maxX; ++x) {
			const float height = snap->GetHeight(x, z);
			if (!HeightsDiffer(terrain->GetAltitudeAtVertex(x, z), height)) {
				continue;
			}

			terrain->SetAltitudeAtVertex(x, z, height);

			changed = true;
			minChangedX = std::min(minChangedX, x);
			minChangedZ = std::min(minChangedZ, z);
			maxChangedX = std::max(maxChangedX, x);
			maxChangedZ = std::max(maxChangedZ, z);
		}
	}

	if (!changed) {
		LOG_INFO(
			"SnapshotManager: region restore [{},{} - {},{}] from '{}' skipped; terrain already matched",
			minX, minZ, maxX, maxZ, snap->name);
		return;
	}

	const SC4Rect<int32_t> rect = CellRectForChangedVertices(
		terrain,
		minChangedX,
		minChangedZ,
		maxChangedX,
		maxChangedZ);
	terrain->RedisplayTerrain(true, true, rect, 0);

	LOG_INFO(
		"SnapshotManager: restored region [{},{} - {},{}] from '{}' with dirty rect [{},{} - {},{}]",
		minX, minZ, maxX, maxZ, snap->name,
		rect.topLeftX, rect.topLeftY, rect.bottomRightX, rect.bottomRightY);
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

bool SnapshotManager::TerrainDiffersFromLatest(cISTETerrain* terrain) const {
	if (!terrain) return false;
	if (snapshots_.empty()) return true;

	const TerrainSnapshot& latest = snapshots_.back();
	const uint32_t cx = terrain->CellCountX() + 1;
	const uint32_t cz = terrain->CellCountZ() + 1;
	if (latest.vertexCountX != cx || latest.vertexCountZ != cz) {
		return true;
	}

	for (uint32_t z = 0; z < cz; ++z) {
		for (uint32_t x = 0; x < cx; ++x) {
			const auto xi = static_cast<int>(x);
			const auto zi = static_cast<int>(z);
			if (HeightsDiffer(terrain->GetAltitudeAtVertex(xi, zi), latest.GetHeight(xi, zi))) {
				return true;
			}
		}
	}

	return false;
}
