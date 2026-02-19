#include "BridgeApproachTool.hpp"
#include "SC4Rect.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cmath>

BridgeApproachTool::BridgeApproachTool(cISTETerrain* terrain) : TerrainOperator(terrain) {}


void BridgeApproachTool::CreateBridgeApproaches(
	int startTileX, int startTileZ,
	int endTileX, int endTileZ,
	float bridgeHeight,
	float approachLength,
	float maxGrade,
	float widthTiles,
	bool useTapering) {
	const int bridgeDx = endTileX - startTileX;
	const int bridgeDz = endTileZ - startTileZ;
	const float bridgeLength = std::sqrt(
		static_cast<float>(bridgeDx * bridgeDx + bridgeDz * bridgeDz));

	if (bridgeLength == 0.0f) {
		LOG_DEBUG("BridgeApproachTool: start and end are the same point");
		return;
	}

	// Unit direction vector along the bridge span
	const float dirX = bridgeDx / bridgeLength;
	const float dirZ = bridgeDz / bridgeLength;

	// Each approach extends *away* from the bridge
	const float startDirX = -dirX;
	const float startDirZ = -dirZ;
	const float endDirX = dirX;
	const float endDirZ = dirZ;

	LOG_DEBUG("BridgeApproachTool: span ({},{}) -> ({},{}) "
	          "length={:.2f} dir=({:.3f},{:.3f})",
	          startTileX, startTileZ, endTileX, endTileZ,
	          bridgeLength, dirX, dirZ);

	const uint32_t maxX = terrain_->CellCountX() - 1;
	const uint32_t maxZ = terrain_->CellCountZ() - 1;

	// Start approach
	{
		float len = approachLength;
		if (len < 0.0f) {
			len = CalculateOptimalApproachLength_(
				startTileX, startTileZ,
				startDirX, startDirZ,
				bridgeHeight, maxGrade);
		}

		const int endX = std::clamp(
			static_cast<int>(startTileX + startDirX * len),
			0, static_cast<int>(maxX));
		const int endZ = std::clamp(
			static_cast<int>(startTileZ + startDirZ * len),
			0, static_cast<int>(maxZ));

		const float groundHeight = GetTileAverageHeight(endX, endZ);

		LOG_DEBUG("BridgeApproachTool: start approach -> ({},{}) "
		          "groundHeight={:.2f} length={:.1f}",
		          endX, endZ, groundHeight, len);

		CreateSingleApproach_(
			startTileX, startTileZ,
			endX, endZ,
			bridgeHeight, groundHeight,
			widthTiles, "start",
			useTapering);
	}

	// End approach
	{
		float len = approachLength;
		if (len < 0.0f) {
			len = CalculateOptimalApproachLength_(
				endTileX, endTileZ,
				endDirX, endDirZ,
				bridgeHeight, maxGrade);
		}

		const int endX = std::clamp(
			static_cast<int>(endTileX + endDirX * len),
			0, static_cast<int>(maxX));
		const int endZ = std::clamp(
			static_cast<int>(endTileZ + endDirZ * len),
			0, static_cast<int>(maxZ));

		const float groundHeight = GetTileAverageHeight(endX, endZ);

		LOG_DEBUG("BridgeApproachTool: end approach -> ({},{}) "
		          "groundHeight={:.2f} length={:.1f}",
		          endX, endZ, groundHeight, len);

		CreateSingleApproach_(
			endTileX, endTileZ,
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

	int negOffset = 0, posOffset = 0;
	GetWidthOffsetBounds_(GetEffectiveWidthTiles_(widthTiles), negOffset, posOffset);

	if (slopeInX) {
		minTileZ -= negOffset;
		maxTileZ += posOffset;
	}
	else {
		minTileX -= negOffset;
		maxTileX += posOffset;
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

float BridgeApproachTool::CalculateOptimalApproachLength_(
	int bridgeX, int bridgeZ,
	float dirX, float dirZ,
	float bridgeHeight,
	float maxGrade) {
	constexpr int kMaxIterations = 10;
	constexpr float kConvergenceThreshold = 0.5f;
	constexpr float kDamping = 0.3f; // weight on current estimate
	constexpr float kNewWeight = 0.7f; // weight on new estimate
	constexpr float kMinLength = 2.0f;

	float currentLength = 3.0f;

	for (int i = 0; i < kMaxIterations; ++i) {
		int sampleX = bridgeX + static_cast<int>(dirX * currentLength);
		int sampleZ = bridgeZ + static_cast<int>(dirZ * currentLength);
		ClampToTerrainBounds(sampleX, sampleZ);

		const float terrainHeight = GetTileAverageHeight(sampleX, sampleZ);
		const float requiredLength = CalculateRequiredApproachLength_(terrainHeight, bridgeHeight, maxGrade);

		if (std::abs(requiredLength - currentLength) < kConvergenceThreshold) {
			LOG_DEBUG("BridgeApproachTool: approach length converged to "
			          "{:.1f} tiles after {} iterations",
			          requiredLength, i + 1);
			return requiredLength;
		}

		currentLength = std::max(
			kMinLength,
			kDamping * currentLength + kNewWeight * requiredLength);
	}

	LOG_WARN("BridgeApproachTool: approach length did not converge, "
	         "using {:.1f} tiles", currentLength);
	return currentLength;
}

float BridgeApproachTool::CalculateRequiredApproachLength_(
	float terrainHeight,
	float bridgeHeight,
	float maxGrade) {
	constexpr float kMetresPerTile = 16.0f;
	constexpr float kSafetyMargin = 1.2f;
	constexpr float kMinLength = 2.0f;

	const float heightDiff = std::abs(bridgeHeight - terrainHeight);
	const float rawTileLength = (heightDiff * 100.0f / maxGrade) / kMetresPerTile;
	return std::max(kMinLength, std::ceil(rawTileLength * kSafetyMargin));
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

	const int effectiveWidth = GetEffectiveWidthTiles_(widthTiles);
	int negOffset = 0, posOffset = 0;
	GetWidthOffsetBounds_(effectiveWidth, negOffset, posOffset);

	const float halfWidth = static_cast<float>(effectiveWidth) / 2.0f;
	const float taperDenominator = static_cast<float>(
		std::max(negOffset, posOffset));

	for (int offset = -negOffset; offset <= posOffset; ++offset) {
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

int BridgeApproachTool::GetEffectiveWidthTiles_(const float widthTiles) const {
	return std::max(1, static_cast<int>(std::round(widthTiles)));
}

void BridgeApproachTool::GetWidthOffsetBounds_(
	const int effectiveWidthTiles,
	int& negativeOffset,
	int& positiveOffset) const {
	negativeOffset = (effectiveWidthTiles - 1) / 2;
	positiveOffset = effectiveWidthTiles / 2;
}
