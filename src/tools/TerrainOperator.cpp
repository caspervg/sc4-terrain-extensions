#include "tools/TerrainOperator.hpp"
#include "cISTETerrain.h"
#include "cISTETerrainMap.h"
#include "SC4Rect.h"
#include <algorithm>

float TerrainOperator::GetTileAverageHeight(int tileX, int tileZ) const {
	Vector3 corners[4];
	GetTileCorners(tileX, tileZ, corners);
	return (corners[0].height + corners[1].height +
		corners[2].height + corners[3].height) / 4.0f;
}

void TerrainOperator::GetTileCorners(
	int tileX, int tileZ, Vector3 corners[4]) const {
	corners[0] = {
		static_cast<float>(tileX), static_cast<float>(tileZ),
		terrain_->GetAltitudeAtVertex(tileX, tileZ)
	};
	corners[1] = {
		static_cast<float>(tileX + 1), static_cast<float>(tileZ),
		terrain_->GetAltitudeAtVertex(tileX + 1, tileZ)
	};
	corners[2] = {
		static_cast<float>(tileX), static_cast<float>(tileZ + 1),
		terrain_->GetAltitudeAtVertex(tileX, tileZ + 1)
	};
	corners[3] = {
		static_cast<float>(tileX + 1), static_cast<float>(tileZ + 1),
		terrain_->GetAltitudeAtVertex(tileX + 1, tileZ + 1)
	};
}

bool TerrainOperator::IsValidTile(int tileX, int tileZ) const {
	return tileX >= 0
		&& tileZ >= 0
		&& static_cast<uint32_t>(tileX) < terrain_->CellCountX()
		&& static_cast<uint32_t>(tileZ) < terrain_->CellCountZ();
}

void TerrainOperator::SetAltitudeAtVertex(int x, int z, float height) {
	if (x >= 0 && z >= 0
		&& static_cast<uint32_t>(x) <= terrain_->CellCountX()
		&& static_cast<uint32_t>(z) <= terrain_->CellCountZ()) {
		terrain_->SetAltitudeAtVertex(x, z, height);
	}
}

void TerrainOperator::Refresh(const SC4Rect<int32_t>& rect) {
	terrain_->RedisplayTerrain(true, true, rect, 0);
}

void TerrainOperator::ClampToTerrainBounds(int& x, int& z) const {
	x = ClampXToTerrainBounds(x);
	z = ClampZToTerrainBounds(z);
}

int TerrainOperator::ClampXToTerrainBounds(int x) const {
	return std::clamp(x, 0, static_cast<int>(terrain_->CellCountX()));
}

int TerrainOperator::ClampZToTerrainBounds(int z) const {
	return std::clamp(z, 0, static_cast<int>(terrain_->CellCountZ()));
}
