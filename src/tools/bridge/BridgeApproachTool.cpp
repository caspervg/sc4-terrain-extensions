#include "BridgeApproachTool.hpp"
#include "BridgeApproachGeometry.hpp"
#include "BridgePlacement.hpp"
#include "SC4Rect.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cmath>

BridgeApproachTool::BridgeApproachTool(cISTETerrain* terrain) : TerrainOperator(terrain) {}

float BridgeApproachTool::SampleTileHeight_(void* context, const int tileX, const int tileZ) {
	const auto* tool = static_cast<BridgeApproachTool*>(context);
	return BridgeApproachGeometry::SampleTileAverageHeight(tool->terrain_, tileX, tileZ);
}


void BridgeApproachTool::CreateBridgeApproaches(
	int startTileX, int startTileZ,
	int endTileX, int endTileZ,
	float bridgeHeight,
	float approachLength,
	float maxGrade,
	float widthTiles,
	bool useTapering) {
	const auto placement = ComputeBridgePlacement(
		startTileX, startTileZ,
		endTileX, endTileZ
	);
	if (!placement.has_value()) {
		LOG_DEBUG("BridgeApproachTool: start and end are the same point");
		return;
	}

	const int maxTileX = static_cast<int>(terrain_->CellCountX()) - 1;
	const int maxTileZ = static_cast<int>(terrain_->CellCountZ()) - 1;

	const auto geometry = BridgeApproachGeometry::BuildApproachParams(
		*placement,
		bridgeHeight,
		maxGrade,
		widthTiles,
		maxTileX,
		maxTileZ,
		&BridgeApproachTool::SampleTileHeight_,
		this
	);

	LOG_DEBUG("BridgeApproachTool: span ({},{}) -> ({},{}) length={:.2f} dir=({:.3f},{:.3f})",
	          placement->bridgeStartX, placement->bridgeStartZ,
	          placement->bridgeEndX, placement->bridgeEndZ,
	          placement->length, geometry.endDirX, geometry.endDirZ);

	// Start approach
	{
		const float len = (approachLength < 0.0f)
			? geometry.startApproachLength
			: approachLength;

		const int endX = std::clamp(
			static_cast<int>(placement->bridgeStartX + geometry.startDirX * len),
			0, maxTileX);
		const int endZ = std::clamp(
			static_cast<int>(placement->bridgeStartZ + geometry.startDirZ * len),
			0, maxTileZ);

		const float groundHeight = BridgeApproachGeometry::SampleTileAverageHeight(
			terrain_, endX, endZ);

		LOG_DEBUG("BridgeApproachTool: start approach -> ({},{}) "
		          "groundHeight={:.2f} length={:.1f}",
		          endX, endZ, groundHeight, len);

		CreateSingleApproach_(
			placement->bridgeStartX, placement->bridgeStartZ,
			endX, endZ,
			bridgeHeight, groundHeight,
			widthTiles, "start",
			useTapering);
	}

	// End approach
	{
		const float len = (approachLength < 0.0f)
			? geometry.endApproachLength
			: approachLength;

		const int endX = std::clamp(
			static_cast<int>(placement->bridgeEndX + geometry.endDirX * len),
			0, maxTileX);
		const int endZ = std::clamp(
			static_cast<int>(placement->bridgeEndZ + geometry.endDirZ * len),
			0, maxTileZ);

		const float groundHeight = BridgeApproachGeometry::SampleTileAverageHeight(
			terrain_, endX, endZ);

		LOG_DEBUG("BridgeApproachTool: end approach -> ({},{}) "
		          "groundHeight={:.2f} length={:.1f}",
		          endX, endZ, groundHeight, len);

		CreateSingleApproach_(
			placement->bridgeEndX, placement->bridgeEndZ,
			endX, endZ,
			bridgeHeight, groundHeight,
			widthTiles, "end",
			useTapering);
	}
}

void BridgeApproachTool::CreateSingleApproach_(
	int startTileX, int startTileZ,
	int endTileX, int endTileZ,
	float startHeight,
	float endHeight,
	float widthTiles,
	const char* label,
	const bool useTapering) {
	const int tileDx = endTileX - startTileX;
	const int tileDz = endTileZ - startTileZ;
	const int pathLength = std::max(std::abs(tileDx), std::abs(tileDz));

	if (pathLength == 0) return;

	LOG_DEBUG("BridgeApproachTool: {} approach ({},{}) -> ({},{}) "
	          "{:.2f}m -> {:.2f}m over {} tiles",
	          label,
	          startTileX, startTileZ,
	          endTileX, endTileZ,
	          startHeight, endHeight, pathLength);

	const bool slopeInX = std::abs(tileDx) >= std::abs(tileDz);
	const float stepX = static_cast<float>(tileDx) / pathLength;
	const float stepZ = static_cast<float>(tileDz) / pathLength;
	const float heightStep = (endHeight - startHeight) / pathLength;

	// Compute refresh bounds
	int minTileX = std::min(startTileX, endTileX);
	int maxTileX = std::max(startTileX, endTileX);
	int minTileZ = std::min(startTileZ, endTileZ);
	int maxTileZ = std::max(startTileZ, endTileZ);

	const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(
		BridgeApproachGeometry::GetEffectiveWidthTiles(widthTiles));

	if (slopeInX) {
		minTileZ -= widthOffsets.negativeOffset;
		maxTileZ += widthOffsets.positiveOffset;
	}
	else {
		minTileX -= widthOffsets.negativeOffset;
		maxTileX += widthOffsets.positiveOffset;
	}

	// Grade each tile along the path
	for (auto step = 0; step <= pathLength; ++step) {
		const auto tileX = static_cast<int>(
			std::round(startTileX + stepX * step));
		const auto tileZ = static_cast<int>(
			std::round(startTileZ + stepZ * step));
		const float height = startHeight + heightStep * step;

		// On the final tile suppress the internal gradient so we land
		// exactly on endHeight without overshooting
		const float internalStep = (step == pathLength) ? 0.0f : heightStep;

		ApplyGradeToTileWidth_(
			tileX, tileZ, height,
			widthTiles, slopeInX,
			internalStep, useTapering);
	}

	// Refresh the modified region
	Refresh(SC4Rect<int32_t>(
		ClampXToTerrainBounds(minTileX),
		ClampZToTerrainBounds(minTileZ),
		ClampXToTerrainBounds(maxTileX + 1),
		ClampZToTerrainBounds(maxTileZ + 1)
	));
}

void BridgeApproachTool::ApplyGradeToTileWidth_(
	int centerTileX, int centerTileZ,
	float baseHeight,
	float widthTiles,
	bool slopeInX,
	float heightStep,
	bool useTapering) {
	const int perpDx = slopeInX ? 0 : 1;
	const int perpDz = slopeInX ? 1 : 0;

	const int effectiveWidth = BridgeApproachGeometry::GetEffectiveWidthTiles(widthTiles);
	const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(effectiveWidth);

	const float halfWidth = static_cast<float>(effectiveWidth) / 2.0f;
	const float taperDenominator = static_cast<float>(
		std::max(widthOffsets.negativeOffset, widthOffsets.positiveOffset));

	for (int offset = -widthOffsets.negativeOffset; offset <= widthOffsets.positiveOffset; ++offset) {
		const float distFromCenter = static_cast<float>(std::abs(offset));
		if (distFromCenter > halfWidth) continue;

		float influence = 1.0f;
		if (useTapering && taperDenominator > 0.0f) {
			influence = std::clamp(
				1.0f - (distFromCenter / taperDenominator),
				0.0f, 1.0f);
		}

		ApplyBuildableGradeToTile_(
			centerTileX + perpDx * offset,
			centerTileZ + perpDz * offset,
			baseHeight, influence, slopeInX, heightStep);
	}
}

auto BridgeApproachTool::ApplyBuildableGradeToTile_(
	const int tileX, const int tileZ,
	const float baseHeight,
	const float influence,
	const bool slopeInX,
	const float heightStep) -> void {
	if (!IsValidTile(tileX, tileZ)) return;

	Vector3 corners[4];
	GetTileCorners(tileX, tileZ, corners);

	Vector3 target[4];

	if (slopeInX) {
		// Slope along X — left and right edges differ in height,
		// top and bottom edges of the tile are level with each other
		const float leftH = baseHeight;
		const float rightH = baseHeight + heightStep;
		target[0] = Vector3(tileX, tileZ, leftH);
		target[1] = Vector3(tileX + 1, tileZ, rightH);
		target[2] = Vector3(tileX, tileZ + 1, leftH);
		target[3] = Vector3(tileX + 1, tileZ + 1, rightH);
	}
	else {
		// Slope along Z — bottom and top edges differ in height
		const float bottomH = baseHeight;
		const float topH = baseHeight + heightStep;
		target[0] = Vector3(tileX, tileZ, bottomH);
		target[1] = Vector3(tileX + 1, tileZ, bottomH);
		target[2] = Vector3(tileX, tileZ + 1, topH);
		target[3] = Vector3(tileX + 1, tileZ + 1, topH);
	}

	for (int i = 0; i < 4; ++i) {
		SetAltitudeAtVertex(
			static_cast<int>(target[i].x),
			static_cast<int>(target[i].z),
			Lerp(corners[i].height, target[i].height, influence)
		);
	}
}
