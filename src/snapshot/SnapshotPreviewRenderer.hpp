#pragma once
#include "viz/OverlayRenderer.hpp"

struct TerrainSnapshot;
class cISTETerrain;

class SnapshotPreviewRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerGround  = 0;
    static constexpr uint32_t kLayerFill    = 1;
    static constexpr uint32_t kLayerOutline = 2;
    static constexpr uint32_t kLayerMarkers = 3;

    void Rebuild(const TerrainSnapshot& snapshot, cISTETerrain* currentTerrain);
    void ClearAll();

private:
    static constexpr float kTileSize            = 16.0f;
    static constexpr float kHeightThreshold     = 0.05f;  // tile considered changed
    static constexpr float kMarkerThreshold     = 2.0f;   // vertex needs a marker
    static constexpr float kGroundHeightOffset  = 0.05f;
    static constexpr float kOverlayHeightOffset = 0.20f;
    static constexpr float kRailThickness       = 0.50f;
    static constexpr float kOutlineThickness    = 1.25f;
    static constexpr float kMarkerThickness     = 0.65f;
    static constexpr float kNodeCrossSize       = 1.55f;
    static constexpr DWORD kGroundColor         = 0x4A9A9A9Au;

    static DWORD NodeColorForDelta(float delta);
};
