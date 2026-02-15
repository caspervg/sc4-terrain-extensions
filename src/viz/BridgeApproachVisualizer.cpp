#include "BridgeApproachVisualizer.hpp"
#include <algorithm>
#include <cmath>
#include "cISTETerrain.h"

BridgeApproachVisualizer gBridgeVisualizer;

namespace {
    constexpr float kTerrainOffset = 0.1f;  // Lift above terrain to avoid z-fighting
    constexpr uint32_t kZBias = 1;

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
        // Calculate perpendicular vector for line width
        float dx = end.x - start.x;
        float dz = end.z - start.z;
        float len = std::sqrt(dx * dx + dz * dz);

        if (len < 0.001f) return;

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
        // Using smoothstep for natural curve
        float smoothT = t * t * (3.0f - 2.0f * t);
        return terrainHeight + (bridgeHeight - terrainHeight) * smoothT;
    }

    DWORD GetGradeColor(float grade, float maxGrade) {
        if (grade <= maxGrade * 0.8f) {
            return kGradeOkColor;
        } else if (grade <= maxGrade) {
            return kGradeWarningColor;
        } else {
            return kGradeErrorColor;
        }
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

    DWORD baseColor = isValid ? kApproachColor : kInvalidColor;

    // Determine bridge orientation
    int32_t dx = endX - startX;
    int32_t dz = endZ - startZ;
    bool isHorizontal = abs(dx) >= abs(dz);

    // Get bridge endpoints in world coordinates
    float bridgeStartX = std::min(startX, endX) * 16.0f;
    float bridgeStartZ = std::min(startZ, endZ) * 16.0f;
    float bridgeEndX = std::max(startX, endX) * 16.0f;
    float bridgeEndZ = std::max(startZ, endZ) * 16.0f;

    if (isHorizontal) {
        bridgeStartZ = (startZ + endZ) * 8.0f;  // Center
        bridgeEndZ = bridgeStartZ;
    } else {
        bridgeStartX = (startX + endX) * 8.0f;  // Center
        bridgeEndX = bridgeStartX;
    }

    // Calculate approach direction (away from bridge)
    float dirX = isHorizontal ? -1.0f : 0.0f;
    float dirZ = !isHorizontal ? -1.0f : 0.0f;
    if (dx < 0) dirX = -dirX;
    if (dz < 0) dirZ = -dirZ;

    // Calculate approach length
    float startTerrainHeight = SampleTerrainHeight(terrain, bridgeStartX, bridgeStartZ);
    float approachLength = (std::abs(bridgeHeight - startTerrainHeight) * 100.0f / maxGrade) / 16.0f;
    approachLength = std::max(2.0f, std::ceil(approachLength * 1.2f));  // 20% safety margin

    // Build approach surface mesh
    const int steps = static_cast<int>(approachLength * 4);  // 4 subdivisions per tile
    const float stepSize = approachLength / steps;
    const float halfWidth = width * 8.0f;  // Convert tiles to meters

    for (int i = 0; i < steps; ++i) {
        float t0 = static_cast<float>(i) / steps;
        float t1 = static_cast<float>(i + 1) / steps;

        float dist0 = approachLength * t0 * 16.0f;
        float dist1 = approachLength * t1 * 16.0f;

        float x0 = bridgeStartX + dirX * dist0;
        float z0 = bridgeStartZ + dirZ * dist0;
        float x1 = bridgeStartX + dirX * dist1;
        float z1 = bridgeStartZ + dirZ * dist1;

        float terrain0 = SampleTerrainHeight(terrain, x0, z0);
        float terrain1 = SampleTerrainHeight(terrain, x1, z1);

        float y0 = CalculateApproachHeight(terrain0, bridgeHeight, t0, approachLength);
        float y1 = CalculateApproachHeight(terrain1, bridgeHeight, t1, approachLength);

        // Calculate grade for this segment
        float segmentGrade = std::abs(y1 - y0) / (dist1 - dist0) * 100.0f;
        DWORD color = GetGradeColor(segmentGrade, maxGrade);

        // Perpendicular direction for width
        float perpX = isHorizontal ? 0.0f : 1.0f;
        float perpZ = isHorizontal ? 1.0f : 0.0f;

        // Four corners of this segment
        BridgeVertex v1 = {x0 - perpX * halfWidth, y0, z0 - perpZ * halfWidth, color};
        BridgeVertex v2 = {x0 + perpX * halfWidth, y0, z0 + perpZ * halfWidth, color};
        BridgeVertex v3 = {x1 + perpX * halfWidth, y1, z1 + perpZ * halfWidth, color};
        BridgeVertex v4 = {x1 - perpX * halfWidth, y1, z1 - perpZ * halfWidth, color};

        EmitQuad(v1, v2, v3, v4, color, previewVertices_);
    }
}

void BridgeApproachVisualizer::BuildHeightMarkers(
    cISTETerrain* terrain,
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ,
    float bridgeHeight,
    float approachLength)
{
    heightMarkerVertices_.clear();

    if (!terrain) return;

    // Similar setup as BuildApproachPreview for direction
    int32_t dx = endX - startX;
    int32_t dz = endZ - startZ;
    bool isHorizontal = abs(dx) >= abs(dz);

    float bridgeStartX = std::min(startX, endX) * 16.0f;
    float bridgeStartZ = std::min(startZ, endZ) * 16.0f;

    if (isHorizontal) {
        bridgeStartZ = (startZ + endZ) * 8.0f;
    } else {
        bridgeStartX = (startX + endX) * 8.0f;
    }

    float dirX = isHorizontal ? (dx > 0 ? -1.0f : 1.0f) : 0.0f;
    float dirZ = !isHorizontal ? (dz > 0 ? -1.0f : 1.0f) : 0.0f;

    // Place markers every 5 tiles
    const int markerInterval = 5;
    const int numMarkers = static_cast<int>(approachLength / markerInterval) + 1;

    for (int i = 0; i <= numMarkers; ++i) {
        float t = static_cast<float>(i * markerInterval) / approachLength;
        if (t > 1.0f) t = 1.0f;

        float dist = approachLength * t * 16.0f;
        float x = bridgeStartX + dirX * dist;
        float z = bridgeStartZ + dirZ * dist;

        float terrainY = SampleTerrainHeight(terrain, x, z);
        float approachY = CalculateApproachHeight(terrainY, bridgeHeight, t, approachLength);

        // Draw vertical line from terrain to approach
        BridgeVertex bottom = {x, terrainY, z, kHeightMarkerColor};
        BridgeVertex top = {x, approachY, z, kHeightMarkerColor};

        EmitLine(bottom, top, 0.5f, kHeightMarkerColor, heightMarkerVertices_);

        // Add small horizontal cap at top
        float capSize = 1.0f;
        BridgeVertex capLeft = {x - capSize, approachY, z, kHeightMarkerColor};
        BridgeVertex capRight = {x + capSize, approachY, z, kHeightMarkerColor};
        EmitLine(capLeft, capRight, 0.3f, kHeightMarkerColor, heightMarkerVertices_);
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
    // Already handled in BuildApproachPreview via GetGradeColor
    // This could be used for additional grade indicators like arrows or text
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

    if (previewVertices_.empty() && heightMarkerVertices_.empty()) return;

    // Save render state (similar to road decals)
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

    // Restore render state
    device->SetRenderState(D3DRENDERSTATE_ZENABLE, oldZEnable);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, oldZWrite);
    device->SetRenderState(D3DRENDERSTATE_LIGHTING, oldLighting);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, oldAlphaBlend);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE, oldCullMode);
    device->SetRenderState(D3DRENDERSTATE_ZBIAS, oldZBias);
}