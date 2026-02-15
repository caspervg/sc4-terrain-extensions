#include "BridgeApproachVisualizer.hpp"
#include <algorithm>
#include <cmath>
#include "cISTETerrain.h"

BridgeApproachVisualizer gBridgeVisualizer;

namespace {
    constexpr float kTerrainOffset = 0.1f;  // Lift above terrain to avoid z-fighting
    constexpr uint32_t kZBias = 1;
    constexpr DWORD kSkeletonColor = 0xD0FFFFFF;

    void EmitQuad(const BridgeVertex& a, const BridgeVertex& b,
                  const BridgeVertex& c, const BridgeVertex& d,
                  DWORD color, std::vector<BridgeVertex>& outVerts) {
        // Triangle 1: a-b-c
        outVerts.push_back({a.x, a.y, a.z, color});
        outVerts.push_back({b.x, b.y, b.z, color});
        outVerts.push_back({c.x, c.y, c.z, color});

        // Triangle 2: a-c-d
        outVerts.push_back({a.x, a.y, a.z, color});
        outVerts.push_back({c.x, c.y, c.z, color});
        outVerts.push_back({d.x, d.y, d.z, color});
    }

    void EmitLine(const BridgeVertex& start, const BridgeVertex& end,
                  float thickness, DWORD color, std::vector<BridgeVertex>& outVerts) {
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float dz = end.z - start.z;
        float len = std::sqrt(dx * dx + dz * dz);

        if (len < 0.001f) {
            // Vertical line — pick an arbitrary perpendicular
            float perpX = thickness * 0.5f;
            float perpZ = 0.0f;

            BridgeVertex p1 = {start.x - perpX, start.y, start.z, color};
            BridgeVertex p2 = {start.x + perpX, start.y, start.z, color};
            BridgeVertex p3 = {end.x + perpX, end.y, end.z, color};
            BridgeVertex p4 = {end.x - perpX, end.y, end.z, color};

            EmitQuad(p1, p2, p3, p4, color, outVerts);
            return;
        }

        float perpX = -dz / len * thickness * 0.5f;
        float perpZ = dx / len * thickness * 0.5f;

        BridgeVertex p1 = {start.x - perpX, start.y, start.z - perpZ, color};
        BridgeVertex p2 = {start.x + perpX, start.y, start.z + perpZ, color};
        BridgeVertex p3 = {end.x + perpX, end.y, end.z + perpZ, color};
        BridgeVertex p4 = {end.x - perpX, end.y, end.z - perpZ, color};

        EmitQuad(p1, p2, p3, p4, color, outVerts);
    }

    float SampleTerrainHeight(cISTETerrain* terrain, float worldX, float worldZ) {
        if (!terrain) return 0.0f;
        return terrain->GetAltitudeAtNearestGrid(worldX, worldZ) + kTerrainOffset;
    }

    float CalculateApproachHeight(float terrainHeight, float bridgeHeight,
                                  float t, float approachLength) {
        // Smooth transition from terrain to bridge height
        float smoothT = t * t * (3.0f - 2.0f * t);
        return terrainHeight + (bridgeHeight - terrainHeight) * smoothT;
    }

    // Shared terrain vertices make boundary tiles blend with adjacent outside tiles.
    // Blend in a 1-tile outward sample so preview rails follow that diagonal tendency.
    float SampleBoundaryTerrainHeight(
        cISTETerrain* terrain,
        float worldX,
        float worldZ,
        float outwardDirX,
        float outwardDirZ)
    {
        constexpr float kTileSize = 16.0f;
        const float edgeSample = SampleTerrainHeight(terrain, worldX, worldZ);
        const float outsideSample = SampleTerrainHeight(
            terrain,
            worldX + outwardDirX * kTileSize,
            worldZ + outwardDirZ * kTileSize);
        return (edgeSample + outsideSample) * 0.5f;
    }

    int GetEffectiveWidthTiles(float width) {
        return std::max(1, static_cast<int>(std::round(width)));
    }

    void GetWidthOffsetBounds(int effectiveWidthTiles, int& negativeOffset, int& positiveOffset) {
        negativeOffset = (effectiveWidthTiles - 1) / 2;
        positiveOffset = effectiveWidthTiles / 2;
    }

    // Build one approach ramp extending outward from a bridge endpoint.
    //  bridgeEndX/Z   - the bridge span endpoint (world coords) where this approach starts
    //  dirX/dirZ      - unit direction pointing AWAY from the bridge
    //  perpX/perpZ    - unit perpendicular for width
    //  bridgeHeight   - height of the bridge deck
    //  maxGrade       - max grade for colouring
    //  halfWidth      - half-width in world units
    //  approachLength - length in tiles
    //  terrain        - terrain pointer
    //  outVerts       - destination vertex buffer
    void BuildSingleApproachGeometry(
        cISTETerrain* terrain,
        float bridgeEndX, float bridgeEndZ,
        float dirX, float dirZ,
        float perpX, float perpZ,
        float bridgeHeight,
        float halfWidth,
        float approachLength,
        std::vector<BridgeVertex>& outVerts)
    {
        const int steps = static_cast<int>(approachLength * 4);  // 4 subdivisions per tile
        if (steps <= 0) return;
        const float railThickness = 1.5f;
        const float rungThickness = 1.0f;
        const float sideRailThickness = 0.50f;
        const float sideStrutThickness = 0.50f;
        constexpr float kOutsideTileOffset = 16.0f;

        for (int i = 0; i < steps; ++i) {
            // t goes from 0 (at bridge deck) to 1 (at terrain level, far from bridge)
            float t0 = static_cast<float>(i) / steps;
            float t1 = static_cast<float>(i + 1) / steps;

            float dist0 = approachLength * t0 * 16.0f;
            float dist1 = approachLength * t1 * 16.0f;

            float x0 = bridgeEndX + dirX * dist0;
            float z0 = bridgeEndZ + dirZ * dist0;
            float x1 = bridgeEndX + dirX * dist1;
            float z1 = bridgeEndZ + dirZ * dist1;

            float leftX0 = x0 - perpX * halfWidth;
            float leftZ0 = z0 - perpZ * halfWidth;
            float rightX0 = x0 + perpX * halfWidth;
            float rightZ0 = z0 + perpZ * halfWidth;
            float leftX1 = x1 - perpX * halfWidth;
            float leftZ1 = z1 - perpZ * halfWidth;
            float rightX1 = x1 + perpX * halfWidth;
            float rightZ1 = z1 + perpZ * halfWidth;
            float leftToeX0 = leftX0 - perpX * kOutsideTileOffset;
            float leftToeZ0 = leftZ0 - perpZ * kOutsideTileOffset;
            float rightToeX0 = rightX0 + perpX * kOutsideTileOffset;
            float rightToeZ0 = rightZ0 + perpZ * kOutsideTileOffset;
            float leftToeX1 = leftX1 - perpX * kOutsideTileOffset;
            float leftToeZ1 = leftZ1 - perpZ * kOutsideTileOffset;
            float rightToeX1 = rightX1 + perpX * kOutsideTileOffset;
            float rightToeZ1 = rightZ1 + perpZ * kOutsideTileOffset;

            float leftTerrain0 = SampleBoundaryTerrainHeight(terrain, leftX0, leftZ0, -perpX, -perpZ);
            float rightTerrain0 = SampleBoundaryTerrainHeight(terrain, rightX0, rightZ0, perpX, perpZ);
            float leftTerrain1 = SampleBoundaryTerrainHeight(terrain, leftX1, leftZ1, -perpX, -perpZ);
            float rightTerrain1 = SampleBoundaryTerrainHeight(terrain, rightX1, rightZ1, perpX, perpZ);

            // Heights transition from bridge deck (t=0) down to terrain (t=1).
            float leftY0 = CalculateApproachHeight(leftTerrain0, bridgeHeight, 1.0f - t0, approachLength);
            float rightY0 = CalculateApproachHeight(rightTerrain0, bridgeHeight, 1.0f - t0, approachLength);
            float leftY1 = CalculateApproachHeight(leftTerrain1, bridgeHeight, 1.0f - t1, approachLength);
            float rightY1 = CalculateApproachHeight(rightTerrain1, bridgeHeight, 1.0f - t1, approachLength);
            float leftToeY0 = SampleTerrainHeight(terrain, leftToeX0, leftToeZ0);
            float rightToeY0 = SampleTerrainHeight(terrain, rightToeX0, rightToeZ0);
            float leftToeY1 = SampleTerrainHeight(terrain, leftToeX1, leftToeZ1);
            float rightToeY1 = SampleTerrainHeight(terrain, rightToeX1, rightToeZ1);

            BridgeVertex left0 = {leftX0, leftY0, leftZ0, kSkeletonColor};
            BridgeVertex right0 = {rightX0, rightY0, rightZ0, kSkeletonColor};
            BridgeVertex left1 = {leftX1, leftY1, leftZ1, kSkeletonColor};
            BridgeVertex right1 = {rightX1, rightY1, rightZ1, kSkeletonColor};
            BridgeVertex center0 = {x0, (leftY0 + rightY0) * 0.5f, z0, kSkeletonColor};
            BridgeVertex center1 = {x1, (leftY1 + rightY1) * 0.5f, z1, kSkeletonColor};
            BridgeVertex leftToe0 = {leftToeX0, leftToeY0, leftToeZ0, kSkeletonColor};
            BridgeVertex rightToe0 = {rightToeX0, rightToeY0, rightToeZ0, kSkeletonColor};
            BridgeVertex leftToe1 = {leftToeX1, leftToeY1, leftToeZ1, kSkeletonColor};
            BridgeVertex rightToe1 = {rightToeX1, rightToeY1, rightToeZ1, kSkeletonColor};

            EmitLine(left0, left1, railThickness, kSkeletonColor, outVerts);
            EmitLine(right0, right1, railThickness, kSkeletonColor, outVerts);
            EmitLine(center0, center1, sideRailThickness, kSkeletonColor, outVerts);
            EmitLine(leftToe0, leftToe1, sideRailThickness, kSkeletonColor, outVerts);
            EmitLine(rightToe0, rightToe1, sideRailThickness, kSkeletonColor, outVerts);

            // Add periodic cross-ties for a "skeleton" look.
            if ((i % 4) == 0 || i == (steps - 1)) {
                EmitLine(left0, right0, rungThickness, kSkeletonColor, outVerts);
            }

            // Show embankment slopes from approach edges down to adjacent terrain.
            if ((i % 2) == 0 || i == (steps - 1)) {
                EmitLine(left0, leftToe0, sideStrutThickness, kSkeletonColor, outVerts);
                EmitLine(right0, rightToe0, sideStrutThickness, kSkeletonColor, outVerts);
            }
        }
    }

    // Build height markers for one approach direction
    void BuildSingleApproachHeightMarkers(
        cISTETerrain* terrain,
        float bridgeEndX, float bridgeEndZ,
        float dirX, float dirZ,
        float bridgeHeight,
        float approachLength,
        std::vector<BridgeVertex>& outVerts)
    {
        const int markerInterval = 5;  // Every 5 tiles
        const int numMarkers = static_cast<int>(approachLength / markerInterval) + 1;

        for (int i = 0; i <= numMarkers; ++i) {
            float t = static_cast<float>(i * markerInterval) / approachLength;
            if (t > 1.0f) t = 1.0f;

            float dist = approachLength * t * 16.0f;
            float x = bridgeEndX + dirX * dist;
            float z = bridgeEndZ + dirZ * dist;

            float terrainY = SampleTerrainHeight(terrain, x, z);
            float approachY = CalculateApproachHeight(terrainY, bridgeHeight, 1.0f - t, approachLength);

            // Vertical line from terrain to approach
            BridgeVertex bottom = {x, terrainY, z, kHeightMarkerColor};
            BridgeVertex top = {x, approachY, z, kHeightMarkerColor};
            EmitLine(bottom, top, 0.5f, kHeightMarkerColor, outVerts);

            // Horizontal cap at top
            float capSize = 1.0f;
            BridgeVertex capLeft = {x - capSize, approachY, z, kHeightMarkerColor};
            BridgeVertex capRight = {x + capSize, approachY, z, kHeightMarkerColor};
            EmitLine(capLeft, capRight, 0.3f, kHeightMarkerColor, outVerts);
        }
    }

    // Compute approach parameters from the bridge span definition.
    // Returns: bridgeStartWorld, bridgeEndWorld, perpendicular, approach lengths.
    struct ApproachParams {
        float bridgeStartWorldX, bridgeStartWorldZ;
        float bridgeEndWorldX, bridgeEndWorldZ;
        float startDirX, startDirZ;   // direction away from bridge at start end
        float endDirX, endDirZ;       // direction away from bridge at end end
        float perpX, perpZ;           // perpendicular for width
        float startApproachLength;    // in tiles
        float endApproachLength;      // in tiles
        bool isHorizontal;
    };

    ApproachParams ComputeApproachParams(
        cISTETerrain* terrain,
        int32_t startX, int32_t startZ,
        int32_t endX, int32_t endZ,
        float bridgeHeight, float maxGrade, float width)
    {
        ApproachParams p{};

        int32_t dx = endX - startX;
        int32_t dz = endZ - startZ;
        p.isHorizontal = abs(dx) >= abs(dz);
        const int effectiveWidthTiles = GetEffectiveWidthTiles(width);
        const float evenWidthCenterOffset = (effectiveWidthTiles % 2 == 0) ? 8.0f : 0.0f;

        // Bridge span endpoints in world coordinates
        if (p.isHorizontal) {
            // Bridge runs along X axis
            p.bridgeStartWorldX = (std::min(startX, endX) + 0.5f) * 16.0f;
            p.bridgeEndWorldX   = (std::max(startX, endX) + 0.5f) * 16.0f;
            float centerZ       = (startZ + 0.5f) * 16.0f + evenWidthCenterOffset;
            p.bridgeStartWorldZ = centerZ;
            p.bridgeEndWorldZ   = centerZ;

            // Approaches extend along X, away from bridge
            p.startDirX = -1.0f;  p.startDirZ = 0.0f;  // extends left (negative X)
            p.endDirX   =  1.0f;  p.endDirZ   = 0.0f;  // extends right (positive X)

            p.perpX = 0.0f;  p.perpZ = 1.0f;
        } else {
            // Bridge runs along Z axis
            float centerX       = (startX + 0.5f) * 16.0f + evenWidthCenterOffset;
            p.bridgeStartWorldX = centerX;
            p.bridgeEndWorldX   = centerX;
            p.bridgeStartWorldZ = (std::min(startZ, endZ) + 0.5f) * 16.0f;
            p.bridgeEndWorldZ   = (std::max(startZ, endZ) + 0.5f) * 16.0f;

            p.startDirX = 0.0f;  p.startDirZ = -1.0f;
            p.endDirX   = 0.0f;  p.endDirZ   =  1.0f;

            p.perpX = 1.0f;  p.perpZ = 0.0f;
        }

        // Calculate approach lengths based on height difference and max grade
        float startTerrainH = SampleTerrainHeight(terrain, p.bridgeStartWorldX, p.bridgeStartWorldZ);
        float endTerrainH   = SampleTerrainHeight(terrain, p.bridgeEndWorldX, p.bridgeEndWorldZ);

        auto calcLength = [&](float terrainH) -> float {
            float heightDiff = std::abs(bridgeHeight - terrainH);
            float len = (heightDiff * 100.0f / maxGrade) / 16.0f;
            return std::max(2.0f, std::ceil(len * 1.2f));  // 20% safety margin
        };

        p.startApproachLength = calcLength(startTerrainH);
        p.endApproachLength   = calcLength(endTerrainH);

        return p;
    }
}

void BridgeApproachVisualizer::BuildApproachPreview(
    cISTETerrain* terrain,
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ,
    float bridgeHeight,
    float maxGrade,
    float width,
    bool isValid)
{
    previewVertices_.clear();

    if (!terrain) return;
    (void)isValid;

    ApproachParams p = ComputeApproachParams(terrain, startX, startZ, endX, endZ,
                                              bridgeHeight, maxGrade, width);

    float halfWidth = static_cast<float>(GetEffectiveWidthTiles(width)) * 8.0f;  // tiles to world-unit half-width

    // === Bridge deck outline ===
    {
        const float deckThickness = 0.35f;
        BridgeVertex v1 = {p.bridgeStartWorldX - p.perpX * halfWidth, bridgeHeight + kTerrainOffset,
                           p.bridgeStartWorldZ - p.perpZ * halfWidth, kSkeletonColor};
        BridgeVertex v2 = {p.bridgeStartWorldX + p.perpX * halfWidth, bridgeHeight + kTerrainOffset,
                           p.bridgeStartWorldZ + p.perpZ * halfWidth, kSkeletonColor};
        BridgeVertex v3 = {p.bridgeEndWorldX + p.perpX * halfWidth, bridgeHeight + kTerrainOffset,
                           p.bridgeEndWorldZ + p.perpZ * halfWidth, kSkeletonColor};
        BridgeVertex v4 = {p.bridgeEndWorldX - p.perpX * halfWidth, bridgeHeight + kTerrainOffset,
                           p.bridgeEndWorldZ - p.perpZ * halfWidth, kSkeletonColor};

        EmitLine(v1, v2, deckThickness, kSkeletonColor, previewVertices_);
        EmitLine(v2, v3, deckThickness, kSkeletonColor, previewVertices_);
        EmitLine(v3, v4, deckThickness, kSkeletonColor, previewVertices_);
        EmitLine(v4, v1, deckThickness, kSkeletonColor, previewVertices_);
    }

    // === Start-side approach (extends away from bridgeStart) ===
    BuildSingleApproachGeometry(
        terrain,
        p.bridgeStartWorldX, p.bridgeStartWorldZ,
        p.startDirX, p.startDirZ,
        p.perpX, p.perpZ,
        bridgeHeight, halfWidth,
        p.startApproachLength,
        previewVertices_
    );

    // === End-side approach (extends away from bridgeEnd) ===
    BuildSingleApproachGeometry(
        terrain,
        p.bridgeEndWorldX, p.bridgeEndWorldZ,
        p.endDirX, p.endDirZ,
        p.perpX, p.perpZ,
        bridgeHeight, halfWidth,
        p.endApproachLength,
        previewVertices_
    );
}

void BridgeApproachVisualizer::BuildHeightMarkers(
    cISTETerrain* terrain,
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ,
    float bridgeHeight,
    float maxGrade,
    float width)
{
    heightMarkerVertices_.clear();

    if (!terrain) return;

    ApproachParams p = ComputeApproachParams(terrain, startX, startZ, endX, endZ,
                                              bridgeHeight, maxGrade, width);

    // Height markers on both approaches
    BuildSingleApproachHeightMarkers(
        terrain,
        p.bridgeStartWorldX, p.bridgeStartWorldZ,
        p.startDirX, p.startDirZ,
        bridgeHeight, p.startApproachLength,
        heightMarkerVertices_
    );

    BuildSingleApproachHeightMarkers(
        terrain,
        p.bridgeEndWorldX, p.bridgeEndWorldZ,
        p.endDirX, p.endDirZ,
        bridgeHeight, p.endApproachLength,
        heightMarkerVertices_
    );
}

void BridgeApproachVisualizer::BuildGridOverlay(
    cISTETerrain* terrain,
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ,
    float bridgeHeight,
    float maxGrade,
    float width)
{
    gridVertices_.clear();

    if (!terrain) return;

    ApproachParams p = ComputeApproachParams(terrain, startX, startZ, endX, endZ,
                                              bridgeHeight, maxGrade, width);

    // Determine the full extent in tile space to draw grid lines
    int minTileX, maxTileX, minTileZ, maxTileZ;
    int negativeOffset = 0;
    int positiveOffset = 0;
    GetWidthOffsetBounds(GetEffectiveWidthTiles(width), negativeOffset, positiveOffset);

    if (p.isHorizontal) {
        minTileX = std::min(startX, endX) - static_cast<int>(p.startApproachLength) - 1;
        maxTileX = std::max(startX, endX) + static_cast<int>(p.endApproachLength) + 1;
        minTileZ = startZ - negativeOffset - 1;
        maxTileZ = startZ + positiveOffset + 1;
    } else {
        minTileX = startX - negativeOffset - 1;
        maxTileX = startX + positiveOffset + 1;
        minTileZ = std::min(startZ, endZ) - static_cast<int>(p.startApproachLength) - 1;
        maxTileZ = std::max(startZ, endZ) + static_cast<int>(p.endApproachLength) + 1;
    }

    // Clamp to terrain bounds
    uint32_t terrainMaxX = terrain->CellCountX() - 1;
    uint32_t terrainMaxZ = terrain->CellCountZ() - 1;
    minTileX = std::max(0, minTileX);
    minTileZ = std::max(0, minTileZ);
    maxTileX = std::min(static_cast<int>(terrainMaxX), maxTileX);
    maxTileZ = std::min(static_cast<int>(terrainMaxZ), maxTileZ);

    const float lineThickness = 0.15f;

    // Horizontal grid lines (along X)
    for (int tz = minTileZ; tz <= maxTileZ + 1; ++tz) {
        float worldZ = tz * 16.0f;
        float x0 = minTileX * 16.0f;
        float x1 = (maxTileX + 1) * 16.0f;
        float y0 = SampleTerrainHeight(terrain, x0, worldZ) + kTerrainOffset;
        float y1 = SampleTerrainHeight(terrain, x1, worldZ) + kTerrainOffset;

        BridgeVertex s = {x0, y0, worldZ, kGridColor};
        BridgeVertex e = {x1, y1, worldZ, kGridColor};
        EmitLine(s, e, lineThickness, kGridColor, gridVertices_);
    }

    // Vertical grid lines (along Z)
    for (int tx = minTileX; tx <= maxTileX + 1; ++tx) {
        float worldX = tx * 16.0f;
        float z0 = minTileZ * 16.0f;
        float z1 = (maxTileZ + 1) * 16.0f;
        float y0 = SampleTerrainHeight(terrain, worldX, z0) + kTerrainOffset;
        float y1 = SampleTerrainHeight(terrain, worldX, z1) + kTerrainOffset;

        BridgeVertex s = {worldX, y0, z0, kGridColor};
        BridgeVertex e = {worldX, y1, z1, kGridColor};
        EmitLine(s, e, lineThickness, kGridColor, gridVertices_);
    }
}

void BridgeApproachVisualizer::BuildGradeVisualization(
    cISTETerrain* terrain,
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ,
    float bridgeHeight,
    float maxGrade,
    float width)
{
    // Skeleton preview mode does not render grade colors.
    // This method is kept for future extensions (e.g. numeric grade labels).
}

void BridgeApproachVisualizer::ClearPreview() {
    previewVertices_.clear();
    heightMarkerVertices_.clear();
}

void BridgeApproachVisualizer::ClearAll() {
    previewVertices_.clear();
    activeVertices_.clear();
    gridVertices_.clear();
    heightMarkerVertices_.clear();
}

void BridgeApproachVisualizer::Draw(IDirect3DDevice7* device) {
    if (!device) return;

    if (previewVertices_.empty() && heightMarkerVertices_.empty() && gridVertices_.empty()) return;

    // Save render state
    DWORD oldZEnable, oldZWrite, oldLighting, oldAlphaBlend, oldCullMode, oldZBias;
    device->GetRenderState(D3DRENDERSTATE_ZENABLE, &oldZEnable);
    device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &oldZWrite);
    device->GetRenderState(D3DRENDERSTATE_LIGHTING, &oldLighting);
    device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &oldAlphaBlend);
    device->GetRenderState(D3DRENDERSTATE_CULLMODE, &oldCullMode);
    device->GetRenderState(D3DRENDERSTATE_ZBIAS, &oldZBias);

    // Set render state for transparent overlay
    device->SetRenderState(D3DRENDERSTATE_ZENABLE, TRUE);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRENDERSTATE_ZBIAS, kZBias);

    // Disable texturing
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

    // Draw preview surface
    if (!previewVertices_.empty()) {
        device->DrawPrimitive(
            D3DPT_TRIANGLELIST,
            D3DFVF_XYZ | D3DFVF_DIFFUSE,
            previewVertices_.data(),
            static_cast<DWORD>(previewVertices_.size()),
            D3DDP_WAIT
        );
    }

    // Draw height markers
    if (!heightMarkerVertices_.empty()) {
        device->DrawPrimitive(
            D3DPT_TRIANGLELIST,
            D3DFVF_XYZ | D3DFVF_DIFFUSE,
            heightMarkerVertices_.data(),
            static_cast<DWORD>(heightMarkerVertices_.size()),
            D3DDP_WAIT
        );
    }

    // Draw grid overlay
    if (!gridVertices_.empty()) {
        device->DrawPrimitive(
            D3DPT_TRIANGLELIST,
            D3DFVF_XYZ | D3DFVF_DIFFUSE,
            gridVertices_.data(),
            static_cast<DWORD>(gridVertices_.size()),
            D3DDP_WAIT
        );
    }

    // Restore render state
    device->SetRenderState(D3DRENDERSTATE_ZENABLE, oldZEnable);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, oldZWrite);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING, oldLighting);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE, oldCullMode);
    device->SetRenderState(D3DRENDERSTATE_ZBIAS, oldZBias);
}
