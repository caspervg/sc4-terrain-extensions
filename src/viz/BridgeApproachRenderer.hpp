#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d.h>

#include "cISTETerrain.h"
#include "tools/bridge/BridgeApproachGeometry.hpp"
#include "OverlayRenderer.hpp"

class BridgeApproachRenderer : public OverlayRenderer {
public:
    static constexpr auto kLayerFill = 0u;
    static constexpr auto kLayerOutline = 1u;
    static constexpr auto kLayerMarkers = 2u;

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
        bool forceInvalid
    );

    void EmitNodeMarker_(
        float worldX,
        float worldZ,
        float currentHeight,
        float predictedHeight,
        DWORD color);

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
