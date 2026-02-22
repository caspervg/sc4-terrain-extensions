#pragma once
#include "viz/OverlayRenderer.hpp"

struct TerrainSnapshot;
class cISTETerrain;

class SnapshotPreviewRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerWireframe = 0;

    void Rebuild(const TerrainSnapshot& snapshot, cISTETerrain* currentTerrain);
    void ClearAll();

private:
    static constexpr float kTileSize = 16.0f;
    static constexpr float kHeightThreshold = 0.01f;
    static constexpr float kTerrainOffset = 0.15f;
    static constexpr float kMinLineThickness = 0.4f;
    static constexpr float kMaxLineThickness = 2.5f;
    static constexpr float kMaxDelta = 30.0f; // Full intensity/thickness at 30m

    static DWORD DeltaColor(float delta);
    static float DeltaThickness(float delta);
};
