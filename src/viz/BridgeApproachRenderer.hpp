#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atomic>
#include <d3d.h>
#include <vector>

#include "cISTETerrain.h"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "OverlayRenderer.hpp"

static constexpr uint32_t kApproachColor = 0xA000FF00; // Green, semi-transparent
static constexpr uint32_t kInvalidColor = 0xA0FF0000; // Red, semi-transparent
static constexpr uint32_t kGridColor = 0x30FFFFFF; // White, very transparent
static constexpr uint32_t kHeightMarkerColor = 0x80FFFF00; // Yellow, semi-transparent
static constexpr uint32_t kGradeOkColor = 0xA000FF00; // Green
static constexpr uint32_t kGradeWarningColor = 0xA0FFAA00; // Orange
static constexpr uint32_t kGradeErrorColor = 0xA0FF0000; // Red
static constexpr uint32_t kSkeletonColor = 0xD0FFFFFF;

class BridgeApproachRenderer : public OverlayRenderer {
public:
    static constexpr auto kLayerApproach = 0u;
    static constexpr auto kLayerHeightMarkers = 1u;
    static constexpr auto kLayerGrid = 2u;

    BridgeApproachRenderer() = default;

    void Update(
        cISTETerrain* terrain,
        const BridgeApproachGeometry::ApproachParams& geometry,
        bool showHeightMarkers,
        bool isValid,
        BridgeApproachGeometry::ApproachSideMode sideMode
    );

    void ClearAll();

private:
    void BuildApproachLayer_(
        cISTETerrain* terrain,
        const BridgeApproachGeometry::ApproachParams& geometry,
        bool isValid,
        BridgeApproachGeometry::ApproachSideMode sideMode
    );

    void BuildHeightMarkerLayer_(
        cISTETerrain* terrain,
        const BridgeApproachGeometry::ApproachParams& geometry,
        BridgeApproachGeometry::ApproachSideMode sideMode
    );

    // Single-approach helpers — called twice (start side, end side)
    void BuildSingleApproachGeometry_(
        cISTETerrain* terrain,
        float bridgeEndX, float bridgeEndZ,
        float dirX, float dirZ,
        float perpX, float perpZ,
        float bridgeHeight,
        float halfWidth,
        float approachLength,
        uint32_t layerId,
        DWORD color
    );

    void BuildSingleHeightMarkers_(
        cISTETerrain* terrain,
        float bridgeEndX, float bridgeEndZ,
        float dirX, float dirZ,
        float bridgeHeight,
        float approachLength
    );

    static float SampleTerrainHeight_(
        cISTETerrain* terrain,
        float worldX, float worldZ
    );

    static float SampleBoundaryTerrainHeight_(
        cISTETerrain* terrain,
        float worldX, float worldZ,
        float outwardDirX, float outwardDirZ
    );

    static float CalculateApproachHeight_(
        float terrainHeight, float bridgeHeight,
        float t, float approachLength
    );
};
