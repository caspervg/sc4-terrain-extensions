#include "BridgeApproachRenderer.hpp"
#include "utils/Logger.h"

#include <algorithm>
#include <cmath>

void BridgeApproachRenderer::Update(
	cISTETerrain* terrain,
	const BridgeApproachGeometry::ApproachParams& geometry,
	const bool showHeightMarkers,
	const bool isValid,
	const BridgeApproachGeometry::ApproachSideMode sideMode) {
	if (!terrain) {
		ClearAll();
		return;
	}

	// Always rebuild the approach layer
	this->ClearLayer(kLayerApproach);
	BuildApproachLayer_(terrain, geometry, isValid, sideMode);

	// Height markers — rebuild only if toggled on, clear if off
	this->ClearLayer(kLayerHeightMarkers);
	if (showHeightMarkers) {
		BuildHeightMarkerLayer_(terrain, geometry, sideMode);
	}
}

void BridgeApproachRenderer::ClearAll() {
	this->ClearLayer(kLayerApproach);
	this->ClearLayer(kLayerHeightMarkers);
}

void BridgeApproachRenderer::BuildApproachLayer_(
	cISTETerrain* terrain,
	const BridgeApproachGeometry::ApproachParams& p,
	const bool isValid,
	const BridgeApproachGeometry::ApproachSideMode sideMode) {
	const float halfWidth = static_cast<float>(p.effectiveWidthTiles) * 8.0f;
	const DWORD color = isValid ? kSkeletonColor : kInvalidColor;

	// Bridge deck outline
	{
		constexpr float kDeckThickness = 0.35f;
		constexpr float kTerrainOffset = 0.05f;

		const OverlayVertex v1 = {
			p.bridgeStartWorldX - p.perpX * halfWidth,
			p.bridgeHeight + kTerrainOffset,
			p.bridgeStartWorldZ - p.perpZ * halfWidth
		};
		const OverlayVertex v2 = {
			p.bridgeStartWorldX + p.perpX * halfWidth,
			p.bridgeHeight + kTerrainOffset,
			p.bridgeStartWorldZ + p.perpZ * halfWidth
		};
		const OverlayVertex v3 = {
			p.bridgeEndWorldX + p.perpX * halfWidth,
			p.bridgeHeight + kTerrainOffset,
			p.bridgeEndWorldZ + p.perpZ * halfWidth
		};
		const OverlayVertex v4 = {
			p.bridgeEndWorldX - p.perpX * halfWidth,
			p.bridgeHeight + kTerrainOffset,
			p.bridgeEndWorldZ - p.perpZ * halfWidth
		};

		this->EmitLine(v1, v2, kDeckThickness, color, kLayerApproach);
		this->EmitLine(v2, v3, kDeckThickness, color, kLayerApproach);
		this->EmitLine(v3, v4, kDeckThickness, color, kLayerApproach);
		this->EmitLine(v4, v1, kDeckThickness, color, kLayerApproach);
	}

	if (BridgeApproachGeometry::IncludesStartApproach(sideMode)) {
		BuildSingleApproachGeometry_(
			terrain,
			p.bridgeStartWorldX, p.bridgeStartWorldZ,
			p.startDirX, p.startDirZ,
			p.perpX, p.perpZ,
			p.bridgeHeight, halfWidth,
			p.startApproachLength,
			kLayerApproach, color
		);
	}

	if (BridgeApproachGeometry::IncludesEndApproach(sideMode)) {
		BuildSingleApproachGeometry_(
			terrain,
			p.bridgeEndWorldX, p.bridgeEndWorldZ,
			p.endDirX, p.endDirZ,
			p.perpX, p.perpZ,
			p.bridgeHeight, halfWidth,
			p.endApproachLength,
			kLayerApproach, color
		);
	}
}

void BridgeApproachRenderer::BuildHeightMarkerLayer_(
	cISTETerrain* terrain,
	const BridgeApproachGeometry::ApproachParams& p,
	const BridgeApproachGeometry::ApproachSideMode sideMode) {
	if (BridgeApproachGeometry::IncludesStartApproach(sideMode)) {
		BuildSingleHeightMarkers_(
			terrain,
			p.bridgeStartWorldX, p.bridgeStartWorldZ,
			p.startDirX, p.startDirZ,
			p.bridgeHeight, p.startApproachLength
		);
	}

	if (BridgeApproachGeometry::IncludesEndApproach(sideMode)) {
		BuildSingleHeightMarkers_(
			terrain,
			p.bridgeEndWorldX, p.bridgeEndWorldZ,
			p.endDirX, p.endDirZ,
			p.bridgeHeight, p.endApproachLength
		);
	}
}

void BridgeApproachRenderer::BuildSingleApproachGeometry_(
	cISTETerrain* terrain,
	float bridgeEndX, float bridgeEndZ,
	float dirX, float dirZ,
	float perpX, float perpZ,
	float bridgeHeight,
	float halfWidth,
	float approachLength,
	uint32_t layerId,
	DWORD color) {
	const int steps = static_cast<int>(approachLength * 4);
	if (steps <= 0) return;

	constexpr float kRailThickness = 1.5f;
	constexpr float kRungThickness = 1.0f;
	constexpr float kSideRailThickness = 0.50f;
	constexpr float kSideStrutThickness = 0.50f;
	constexpr float kOutsideTileOffset = 16.0f;

	for (int i = 0; i < steps; ++i) {
		const float t0 = static_cast<float>(i) / steps;
		const float t1 = static_cast<float>(i + 1) / steps;

		const float dist0 = approachLength * t0 * 16.0f;
		const float dist1 = approachLength * t1 * 16.0f;

		const float x0 = bridgeEndX + dirX * dist0;
		const float z0 = bridgeEndZ + dirZ * dist0;
		const float x1 = bridgeEndX + dirX * dist1;
		const float z1 = bridgeEndZ + dirZ * dist1;

		// Edge positions
		const float leftX0 = x0 - perpX * halfWidth;
		const float leftZ0 = z0 - perpZ * halfWidth;
		const float rightX0 = x0 + perpX * halfWidth;
		const float rightZ0 = z0 + perpZ * halfWidth;
		const float leftX1 = x1 - perpX * halfWidth;
		const float leftZ1 = z1 - perpZ * halfWidth;
		const float rightX1 = x1 + perpX * halfWidth;
		const float rightZ1 = z1 + perpZ * halfWidth;

		// Embankment toe positions (one tile out from edge)
		const float leftToeX0 = leftX0 - perpX * kOutsideTileOffset;
		const float leftToeZ0 = leftZ0 - perpZ * kOutsideTileOffset;
		const float rightToeX0 = rightX0 + perpX * kOutsideTileOffset;
		const float rightToeZ0 = rightZ0 + perpZ * kOutsideTileOffset;
		const float leftToeX1 = leftX1 - perpX * kOutsideTileOffset;
		const float leftToeZ1 = leftZ1 - perpZ * kOutsideTileOffset;
		const float rightToeX1 = rightX1 + perpX * kOutsideTileOffset;
		const float rightToeZ1 = rightZ1 + perpZ * kOutsideTileOffset;

		// Sample terrain heights
		const float leftTerrain0 = SampleBoundaryTerrainHeight_(terrain, leftX0, leftZ0, -perpX, -perpZ);
		const float rightTerrain0 = SampleBoundaryTerrainHeight_(terrain, rightX0, rightZ0, perpX, perpZ);
		const float leftTerrain1 = SampleBoundaryTerrainHeight_(terrain, leftX1, leftZ1, -perpX, -perpZ);
		const float rightTerrain1 = SampleBoundaryTerrainHeight_(terrain, rightX1, rightZ1, perpX, perpZ);

		// Approach heights — t=0 at bridge deck, t=1 at terrain
		const float leftY0 = CalculateApproachHeight_(leftTerrain0, bridgeHeight, 1.0f - t0, approachLength);
		const float rightY0 = CalculateApproachHeight_(rightTerrain0, bridgeHeight, 1.0f - t0, approachLength);
		const float leftY1 = CalculateApproachHeight_(leftTerrain1, bridgeHeight, 1.0f - t1, approachLength);
		const float rightY1 = CalculateApproachHeight_(rightTerrain1, bridgeHeight, 1.0f - t1, approachLength);

		// Toe heights follow terrain directly
		const float leftToeY0 = SampleTerrainHeight_(terrain, leftToeX0, leftToeZ0);
		const float rightToeY0 = SampleTerrainHeight_(terrain, rightToeX0, rightToeZ0);
		const float leftToeY1 = SampleTerrainHeight_(terrain, leftToeX1, leftToeZ1);
		const float rightToeY1 = SampleTerrainHeight_(terrain, rightToeX1, rightToeZ1);

		const float centerY0 = (leftY0 + rightY0) * 0.5f;
		const float centerY1 = (leftY1 + rightY1) * 0.5f;

		const bool showLeft = true;
		const bool showRight = true;

		this->EmitLine({x0, centerY0, z0},
		                   {x1, centerY1, z1},
		                   kSideRailThickness, color, layerId);

		if (showLeft) {
			this->EmitLine({leftX0, leftY0, leftZ0},
			                   {leftX1, leftY1, leftZ1},
			                   kRailThickness, color, layerId);
			this->EmitLine({leftToeX0, leftToeY0, leftToeZ0},
			                   {leftToeX1, leftToeY1, leftToeZ1},
			                   kSideRailThickness, color, layerId);
		}

		if (showRight) {
			this->EmitLine({rightX0, rightY0, rightZ0},
			                   {rightX1, rightY1, rightZ1},
			                   kRailThickness, color, layerId);
			this->EmitLine({rightToeX0, rightToeY0, rightToeZ0},
			                   {rightToeX1, rightToeY1, rightToeZ1},
			                   kSideRailThickness, color, layerId);
		}

		if ((i % 4) == 0 || i == (steps - 1)) {
			if (showLeft && showRight) {
				this->EmitLine({leftX0, leftY0, leftZ0},
				                   {rightX0, rightY0, rightZ0},
				                   kRungThickness, color, layerId);
			}
			else if (showLeft) {
				this->EmitLine({leftX0, leftY0, leftZ0},
				                   {x0, centerY0, z0},
				                   kRungThickness, color, layerId);
			}
			else if (showRight) {
				this->EmitLine({x0, centerY0, z0},
				                   {rightX0, rightY0, rightZ0},
				                   kRungThickness, color, layerId);
			}
		}

		if ((i % 2) == 0 || i == (steps - 1)) {
			if (showLeft) {
				this->EmitLine({leftX0, leftY0, leftZ0},
				                   {leftToeX0, leftToeY0, leftToeZ0},
				                   kSideStrutThickness, color, layerId);
			}
			if (showRight) {
				this->EmitLine({rightX0, rightY0, rightZ0},
				                   {rightToeX0, rightToeY0, rightToeZ0},
				                   kSideStrutThickness, color, layerId);
			}
		}
	}
}

void BridgeApproachRenderer::BuildSingleHeightMarkers_(
	cISTETerrain* terrain,
	float bridgeEndX, float bridgeEndZ,
	float dirX, float dirZ,
	float bridgeHeight,
	float approachLength) {
	constexpr int kMarkerInterval = 5;
	constexpr float kMarkerThickness = 0.5f;
	constexpr float kCapSize = 1.0f;
	constexpr float kCapThickness = 0.3f;

	const int numMarkers = static_cast<int>(approachLength / kMarkerInterval) + 1;

	for (int i = 0; i <= numMarkers; ++i) {
		float t = static_cast<float>(i * kMarkerInterval) / approachLength;
		t = std::min(t, 1.0f);

		const float dist = approachLength * t * 16.0f;
		const float x = bridgeEndX + dirX * dist;
		const float z = bridgeEndZ + dirZ * dist;
		const float terrainY = SampleTerrainHeight_(terrain, x, z);
		const float approachY = CalculateApproachHeight_(
			terrainY, bridgeHeight, 1.0f - t, approachLength);

		// Vertical stem from terrain to approach level
		this->EmitLine(
			{x, terrainY, z},
			{x, approachY, z},
			kMarkerThickness, kHeightMarkerColor, kLayerHeightMarkers
		);

		// Horizontal cap at top
		this->EmitLine(
			{x - kCapSize, approachY, z},
			{x + kCapSize, approachY, z},
			kCapThickness, kHeightMarkerColor, kLayerHeightMarkers
		);
	}
}

float BridgeApproachRenderer::SampleTerrainHeight_(
	cISTETerrain* terrain, const float worldX, const float worldZ) {
	if (!terrain) return 0.0f;
	constexpr auto kTerrainOffset = 0.1f;
	return terrain->GetAltitudeAtNearestGrid(worldX, worldZ) + kTerrainOffset;
}

float BridgeApproachRenderer::SampleBoundaryTerrainHeight_(
	cISTETerrain* terrain,
	const float worldX, const float worldZ,
	const float outwardDirX, const float outwardDirZ) {
	constexpr auto kTileSize = 16.0f;
	const float edgeSample = SampleTerrainHeight_(terrain, worldX, worldZ);
	const float outsideSample = SampleTerrainHeight_(
		terrain,
		worldX + outwardDirX * kTileSize,
		worldZ + outwardDirZ * kTileSize
	);
	return (edgeSample + outsideSample) * 0.5f;
}

float BridgeApproachRenderer::CalculateApproachHeight_(
	const float terrainHeight, const float bridgeHeight,
	const float t, float approachLength) {
	return terrainHeight + (bridgeHeight - terrainHeight) * t;
}
