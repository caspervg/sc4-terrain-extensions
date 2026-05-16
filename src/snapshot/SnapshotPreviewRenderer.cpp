#include "SnapshotPreviewRenderer.hpp"
#include "TerrainSnapshot.hpp"
#include "cISTETerrain.h"

#include <algorithm>
#include <cmath>
#include <vector>

DWORD SnapshotPreviewRenderer::NodeColorForDelta(const float delta) {
	const float magnitude = std::min(std::abs(delta), 20.0f) / 20.0f;
	const uint8_t alpha = static_cast<uint8_t>(128 + magnitude * 110.0f);

	if (delta > 0.05f) {
		const uint8_t green = static_cast<uint8_t>(165 + magnitude * 70.0f);
		return (static_cast<DWORD>(alpha) << 24)
			| (0x20u << 16)
			| (static_cast<DWORD>(green) << 8)
			| 0x48u;
	}

	if (delta < -0.05f) {
		const uint8_t red = static_cast<uint8_t>(185 + magnitude * 55.0f);
		return (static_cast<DWORD>(alpha) << 24)
			| (static_cast<DWORD>(red) << 16)
			| (0x48u << 8)
			| 0x48u;
	}

	return 0x70C8C8C8u;
}

void SnapshotPreviewRenderer::Rebuild(const TerrainSnapshot& snapshot, cISTETerrain* currentTerrain) {
	ClearAll();
	if (!currentTerrain) return;

	const int vertsX = static_cast<int>(snapshot.vertexCountX);
	const int vertsZ = static_cast<int>(snapshot.vertexCountZ);
	const int cellsX = vertsX - 1;
	const int cellsZ = vertsZ - 1;

	// Pre-compute per-vertex deltas (snapshot height - current height)
	std::vector<float> deltas(static_cast<size_t>(vertsX) * vertsZ);
	std::vector<float> snapH(static_cast<size_t>(vertsX) * vertsZ);
	std::vector<float> curH(static_cast<size_t>(vertsX) * vertsZ);
	for (int z = 0; z < vertsZ; ++z) {
		for (int x = 0; x < vertsX; ++x) {
			const size_t i = static_cast<size_t>(z) * vertsX + x;
			snapH[i] = snapshot.GetHeight(x, z);
			curH[i]  = currentTerrain->GetAltitudeAtVertex(x, z);
			deltas[i] = snapH[i] - curH[i];
		}
	}

	const auto idx = [&](int x, int z) { return static_cast<size_t>(z) * vertsX + x; };

	const auto isTileChanged = [&](int tx, int tz) -> bool {
		if (tx < 0 || tz < 0 || tx >= cellsX || tz >= cellsZ) return false;
		return std::abs(deltas[idx(tx,   tz  )]) > kHeightThreshold
			|| std::abs(deltas[idx(tx+1, tz  )]) > kHeightThreshold
			|| std::abs(deltas[idx(tx,   tz+1)]) > kHeightThreshold
			|| std::abs(deltas[idx(tx+1, tz+1)]) > kHeightThreshold;
	};

	for (int tz = 0; tz < cellsZ; ++tz) {
		for (int tx = 0; tx < cellsX; ++tx) {
			if (!isTileChanged(tx, tz)) continue;

			const float wx0 = tx       * kTileSize;
			const float wx1 = (tx + 1) * kTileSize;
			const float wz0 = tz       * kTileSize;
			const float wz1 = (tz + 1) * kTileSize;

			const float c00 = curH[idx(tx,   tz  )];
			const float c10 = curH[idx(tx+1, tz  )];
			const float c01 = curH[idx(tx,   tz+1)];
			const float c11 = curH[idx(tx+1, tz+1)];

			const float s00 = snapH[idx(tx,   tz  )];
			const float s10 = snapH[idx(tx+1, tz  )];
			const float s01 = snapH[idx(tx,   tz+1)];
			const float s11 = snapH[idx(tx+1, tz+1)];

			const float d00 = deltas[idx(tx,   tz  )];
			const float d10 = deltas[idx(tx+1, tz  )];
			const float d01 = deltas[idx(tx,   tz+1)];
			const float d11 = deltas[idx(tx+1, tz+1)];

			// Ground: gray filled quad at current terrain height
			EmitQuad(
				{wx0, c00 + kGroundHeightOffset, wz0, kGroundColor},
				{wx1, c10 + kGroundHeightOffset, wz0, kGroundColor},
				{wx1, c11 + kGroundHeightOffset, wz1, kGroundColor},
				{wx0, c01 + kGroundHeightOffset, wz1, kGroundColor},
				kGroundColor, kLayerGround);

			// Fill: per-tile rail lines at snapshot height, colored by avg delta
			const DWORD fillColor = NodeColorForDelta((d00 + d10 + d01 + d11) * 0.25f);
			EmitLine({wx0, s00 + kOverlayHeightOffset, wz0, fillColor},
			         {wx1, s10 + kOverlayHeightOffset, wz0, fillColor},
			         kRailThickness, fillColor, kLayerFill);
			EmitLine({wx1, s10 + kOverlayHeightOffset, wz0, fillColor},
			         {wx1, s11 + kOverlayHeightOffset, wz1, fillColor},
			         kRailThickness, fillColor, kLayerFill);
			EmitLine({wx1, s11 + kOverlayHeightOffset, wz1, fillColor},
			         {wx0, s01 + kOverlayHeightOffset, wz1, fillColor},
			         kRailThickness, fillColor, kLayerFill);
			EmitLine({wx0, s01 + kOverlayHeightOffset, wz1, fillColor},
			         {wx0, s00 + kOverlayHeightOffset, wz0, fillColor},
			         kRailThickness, fillColor, kLayerFill);

			// Outline: thicker border only on edges bordering an unchanged tile
			const float ol = kOverlayHeightOffset + 0.02f;
			const DWORD outColor = NodeColorForDelta((d00 + d10 + d01 + d11) * 0.25f);
			if (!isTileChanged(tx, tz - 1))
				EmitLine({wx0, s00+ol, wz0, outColor}, {wx1, s10+ol, wz0, outColor}, kOutlineThickness, outColor, kLayerOutline);
			if (!isTileChanged(tx + 1, tz))
				EmitLine({wx1, s10+ol, wz0, outColor}, {wx1, s11+ol, wz1, outColor}, kOutlineThickness, outColor, kLayerOutline);
			if (!isTileChanged(tx, tz + 1))
				EmitLine({wx1, s11+ol, wz1, outColor}, {wx0, s01+ol, wz1, outColor}, kOutlineThickness, outColor, kLayerOutline);
			if (!isTileChanged(tx - 1, tz))
				EmitLine({wx0, s01+ol, wz1, outColor}, {wx0, s00+ol, wz0, outColor}, kOutlineThickness, outColor, kLayerOutline);
		}
	}

	// Markers: vertical stick + cross only for vertices with significant delta
	for (int z = 0; z < vertsZ; ++z) {
		for (int x = 0; x < vertsX; ++x) {
			const float d = deltas[idx(x, z)];
			if (std::abs(d) < kMarkerThreshold) continue;

			const float wx      = x * kTileSize;
			const float wz      = z * kTileSize;
			const float groundY = curH[idx(x, z)] + 0.03f;
			const float snapY   = snapH[idx(x, z)] + kOverlayHeightOffset;
			const DWORD color   = NodeColorForDelta(d);

			EmitLine({wx, groundY, wz, color}, {wx, snapY, wz, color},
			         kMarkerThickness, color, kLayerMarkers);
			EmitLine({wx - kNodeCrossSize, snapY, wz, color},
			         {wx + kNodeCrossSize, snapY, wz, color},
			         kMarkerThickness, color, kLayerMarkers);
			EmitLine({wx, snapY, wz - kNodeCrossSize, color},
			         {wx, snapY, wz + kNodeCrossSize, color},
			         kMarkerThickness, color, kLayerMarkers);
		}
	}
}

void SnapshotPreviewRenderer::ClearAll() {
	ClearLayer(kLayerGround);
	ClearLayer(kLayerFill);
	ClearLayer(kLayerOutline);
	ClearLayer(kLayerMarkers);
}