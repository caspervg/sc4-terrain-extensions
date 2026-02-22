#include "SnapshotPreviewRenderer.hpp"
#include "TerrainSnapshot.hpp"
#include "cISTETerrain.h"

#include <algorithm>
#include <cmath>

void SnapshotPreviewRenderer::Rebuild(const TerrainSnapshot& snapshot, cISTETerrain* currentTerrain) {
	ClearLayer(kLayerWireframe);

	if (!currentTerrain) return;

	const uint32_t cellsX = snapshot.vertexCountX - 1;
	const uint32_t cellsZ = snapshot.vertexCountZ - 1;

	for (uint32_t tz = 0; tz < cellsZ; ++tz) {
		for (uint32_t tx = 0; tx < cellsX; ++tx) {
			const int x0 = static_cast<int>(tx);
			const int z0 = static_cast<int>(tz);
			const int x1 = x0 + 1;
			const int z1 = z0 + 1;

			const float sh00 = snapshot.GetHeight(x0, z0);
			const float sh10 = snapshot.GetHeight(x1, z0);
			const float sh01 = snapshot.GetHeight(x0, z1);
			const float sh11 = snapshot.GetHeight(x1, z1);

			const float ch00 = currentTerrain->GetAltitudeAtVertex(x0, z0);
			const float ch10 = currentTerrain->GetAltitudeAtVertex(x1, z0);
			const float ch01 = currentTerrain->GetAltitudeAtVertex(x0, z1);
			const float ch11 = currentTerrain->GetAltitudeAtVertex(x1, z1);

			const float d00 = sh00 - ch00;
			const float d10 = sh10 - ch10;
			const float d01 = sh01 - ch01;
			const float d11 = sh11 - ch11;

			const float maxDelta = std::max({
				std::abs(d00), std::abs(d10),
				std::abs(d01), std::abs(d11)
			});
			if (maxDelta < kHeightThreshold) continue;

			const float wx0 = static_cast<float>(x0) * kTileSize;
			const float wx1 = static_cast<float>(x1) * kTileSize;
			const float wz0 = static_cast<float>(z0) * kTileSize;
			const float wz1 = static_cast<float>(z1) * kTileSize;

			const float h00 = sh00 + kTerrainOffset;
			const float h10 = sh10 + kTerrainOffset;
			const float h01 = sh01 + kTerrainOffset;
			const float h11 = sh11 + kTerrainOffset;

			const DWORD c00 = DeltaColor(d00);
			const DWORD c10 = DeltaColor(d10);
			const DWORD c01 = DeltaColor(d01);
			const DWORD c11 = DeltaColor(d11);

			// Line thickness scales with the average delta of each edge's endpoints
			const float t_top = DeltaThickness((d00 + d10) * 0.5f);
			const float t_left = DeltaThickness((d00 + d01) * 0.5f);
			const float t_bottom = DeltaThickness((d01 + d11) * 0.5f);
			const float t_right = DeltaThickness((d10 + d11) * 0.5f);

			// Top edge
			EmitLine({wx0, h00, wz0, c00}, {wx1, h10, wz0, c10},
			         t_top, c00, kLayerWireframe);
			// Left edge
			EmitLine({wx0, h00, wz0, c00}, {wx0, h01, wz1, c01},
			         t_left, c00, kLayerWireframe);
			// Bottom edge
			EmitLine({wx0, h01, wz1, c01}, {wx1, h11, wz1, c11},
			         t_bottom, c01, kLayerWireframe);
			// Right edge
			EmitLine({wx1, h10, wz0, c10}, {wx1, h11, wz1, c11},
			         t_right, c10, kLayerWireframe);
		}
	}
}

void SnapshotPreviewRenderer::ClearAll() {
	Clear();
}

DWORD SnapshotPreviewRenderer::DeltaColor(float delta) {
	// Smooth gradient: green (higher) → yellow (small diff) → red (lower)
	// t goes 0..1 based on magnitude
	const float absDelta = std::abs(delta);
	const float t = std::clamp(absDelta / kMaxDelta, 0.0f, 1.0f);

	// Alpha increases with magnitude: subtle for small diffs, bold for large
	const auto alpha = static_cast<uint8_t>(100 + t * 155); // 100-255

	if (absDelta < kHeightThreshold) {
		// Near-zero: dim yellow
		return (static_cast<DWORD>(80) << 24) | (180u << 16) | (180u << 8);
	}

	if (delta > 0) {
		// Positive (snapshot higher): yellow → green
		// At t=0: yellow (255,255,0), at t=1: green (0,255,0)
		const auto r = static_cast<uint8_t>(255 * (1.0f - t));
		const uint8_t g = 255;
		return (static_cast<DWORD>(alpha) << 24)
			| (static_cast<DWORD>(r) << 16)
			| (static_cast<DWORD>(g) << 8);
	}

	// Negative (snapshot lower): yellow → red
	// At t=0: yellow (255,255,0), at t=1: red (255,0,0)
	const uint8_t r = 255;
	const auto g = static_cast<uint8_t>(255 * (1.0f - t));
	return (static_cast<DWORD>(alpha) << 24)
		| (static_cast<DWORD>(r) << 16)
		| (static_cast<DWORD>(g) << 8);
}

float SnapshotPreviewRenderer::DeltaThickness(float delta) {
	const float t = std::clamp(std::abs(delta) / kMaxDelta, 0.0f, 1.0f);
	return kMinLineThickness + t * (kMaxLineThickness - kMinLineThickness);
}
