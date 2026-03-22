#include "BridgeApproachRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kOverlayHeightOffset = 0.20f;
constexpr float kMarkerThickness = 0.65f;
constexpr float kOutlineThickness = 1.25f;
constexpr float kRailThickness = 0.50f;
constexpr float kDeckThickness = 0.65f;
constexpr float kNodeCrossSize = 1.55f;
constexpr float kRungThickness = 0.90f;
constexpr float kSideStrutThickness = 0.50f;
constexpr float kOutsideTileOffset = 16.0f;
constexpr DWORD kInvalidColor = 0xD0C84A4Au;

DWORD ColorForDelta(const float delta) {
    if (delta > 0.25f) {
        return 0xD030C050;
    }
    if (delta < -0.25f) {
        return 0xD0C84A4A;
    }
    return 0xD0D0D0D0;
}

DWORD NodeColorForDelta(const float delta) {
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
}

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

    ClearLayer(kLayerFill);
    ClearLayer(kLayerOutline);
    BuildApproachLayer_(terrain, geometry, isValid, sideMode);

    ClearLayer(kLayerMarkers);
    if (showHeightMarkers) {
        BuildHeightMarkerLayer_(terrain, geometry, sideMode);
    }
}

void BridgeApproachRenderer::ClearAll() {
    ClearLayer(kLayerFill);
    ClearLayer(kLayerOutline);
    ClearLayer(kLayerMarkers);
}

void BridgeApproachRenderer::BuildApproachLayer_(
    cISTETerrain* terrain,
    const BridgeApproachGeometry::ApproachParams& p,
    const bool isValid,
    const BridgeApproachGeometry::ApproachSideMode sideMode) {
    const float halfWidth = static_cast<float>(p.effectiveWidthTiles) * 8.0f;

    const float leftStartTerrain = SampleBoundaryTerrainHeight_(terrain, p.bridgeStartWorldX - p.perpX * halfWidth, p.bridgeStartWorldZ - p.perpZ * halfWidth, -p.perpX, -p.perpZ);
    const float rightStartTerrain = SampleBoundaryTerrainHeight_(terrain, p.bridgeStartWorldX + p.perpX * halfWidth, p.bridgeStartWorldZ + p.perpZ * halfWidth, p.perpX, p.perpZ);
    const float leftEndTerrain = SampleBoundaryTerrainHeight_(terrain, p.bridgeEndWorldX - p.perpX * halfWidth, p.bridgeEndWorldZ - p.perpZ * halfWidth, -p.perpX, -p.perpZ);
    const float rightEndTerrain = SampleBoundaryTerrainHeight_(terrain, p.bridgeEndWorldX + p.perpX * halfWidth, p.bridgeEndWorldZ + p.perpZ * halfWidth, p.perpX, p.perpZ);
    const float deckDelta = p.bridgeHeight - ((leftStartTerrain + rightStartTerrain + leftEndTerrain + rightEndTerrain) * 0.25f);
    const DWORD deckColor = isValid ? ColorForDelta(deckDelta) : kInvalidColor;

    const OverlayVertex v1 = {
        p.bridgeStartWorldX - p.perpX * halfWidth,
        p.bridgeHeight + kOverlayHeightOffset,
        p.bridgeStartWorldZ - p.perpZ * halfWidth,
        deckColor
    };
    const OverlayVertex v2 = {
        p.bridgeStartWorldX + p.perpX * halfWidth,
        p.bridgeHeight + kOverlayHeightOffset,
        p.bridgeStartWorldZ + p.perpZ * halfWidth,
        deckColor
    };
    const OverlayVertex v3 = {
        p.bridgeEndWorldX + p.perpX * halfWidth,
        p.bridgeHeight + kOverlayHeightOffset,
        p.bridgeEndWorldZ + p.perpZ * halfWidth,
        deckColor
    };
    const OverlayVertex v4 = {
        p.bridgeEndWorldX - p.perpX * halfWidth,
        p.bridgeHeight + kOverlayHeightOffset,
        p.bridgeEndWorldZ - p.perpZ * halfWidth,
        deckColor
    };

    EmitLine(v1, v2, kOutlineThickness, deckColor, kLayerOutline);
    EmitLine(v2, v3, kDeckThickness, deckColor, kLayerOutline);
    EmitLine(v3, v4, kOutlineThickness, deckColor, kLayerOutline);
    EmitLine(v4, v1, kDeckThickness, deckColor, kLayerOutline);

    if (BridgeApproachGeometry::IncludesStartApproach(sideMode)) {
        BuildSingleApproachGeometry_(
            terrain,
            p.bridgeStartWorldX, p.bridgeStartWorldZ,
            p.startDirX, p.startDirZ,
            p.perpX, p.perpZ,
            p.bridgeHeight, halfWidth,
            p.startApproachLength,
            kLayerFill, !isValid);
    }

    if (BridgeApproachGeometry::IncludesEndApproach(sideMode)) {
        BuildSingleApproachGeometry_(
            terrain,
            p.bridgeEndWorldX, p.bridgeEndWorldZ,
            p.endDirX, p.endDirZ,
            p.perpX, p.perpZ,
            p.bridgeHeight, halfWidth,
            p.endApproachLength,
            kLayerFill, !isValid);
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
            p.bridgeHeight, p.startApproachLength);
    }

    if (BridgeApproachGeometry::IncludesEndApproach(sideMode)) {
        BuildSingleHeightMarkers_(
            terrain,
            p.bridgeEndWorldX, p.bridgeEndWorldZ,
            p.endDirX, p.endDirZ,
            p.bridgeHeight, p.endApproachLength);
    }
}

void BridgeApproachRenderer::BuildSingleApproachGeometry_(
    cISTETerrain* terrain,
    const float bridgeEndX, const float bridgeEndZ,
    const float dirX, const float dirZ,
    const float perpX, const float perpZ,
    const float bridgeHeight,
    const float halfWidth,
    const float approachLength,
    const uint32_t layerId,
    const bool forceInvalid) {
    const int steps = static_cast<int>(approachLength * 4);
    if (steps <= 0) return;

    for (int i = 0; i < steps; ++i) {
        const float t0 = static_cast<float>(i) / static_cast<float>(steps);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(steps);

        const float dist0 = approachLength * t0 * 16.0f;
        const float dist1 = approachLength * t1 * 16.0f;

        const float x0 = bridgeEndX + dirX * dist0;
        const float z0 = bridgeEndZ + dirZ * dist0;
        const float x1 = bridgeEndX + dirX * dist1;
        const float z1 = bridgeEndZ + dirZ * dist1;

        const float leftX0 = x0 - perpX * halfWidth;
        const float leftZ0 = z0 - perpZ * halfWidth;
        const float rightX0 = x0 + perpX * halfWidth;
        const float rightZ0 = z0 + perpZ * halfWidth;
        const float leftX1 = x1 - perpX * halfWidth;
        const float leftZ1 = z1 - perpZ * halfWidth;
        const float rightX1 = x1 + perpX * halfWidth;
        const float rightZ1 = z1 + perpZ * halfWidth;

        const float leftToeX0 = leftX0 - perpX * kOutsideTileOffset;
        const float leftToeZ0 = leftZ0 - perpZ * kOutsideTileOffset;
        const float rightToeX0 = rightX0 + perpX * kOutsideTileOffset;
        const float rightToeZ0 = rightZ0 + perpZ * kOutsideTileOffset;
        const float leftToeX1 = leftX1 - perpX * kOutsideTileOffset;
        const float leftToeZ1 = leftZ1 - perpZ * kOutsideTileOffset;
        const float rightToeX1 = rightX1 + perpX * kOutsideTileOffset;
        const float rightToeZ1 = rightZ1 + perpZ * kOutsideTileOffset;

        const float leftTerrain0 = SampleBoundaryTerrainHeight_(terrain, leftX0, leftZ0, -perpX, -perpZ);
        const float rightTerrain0 = SampleBoundaryTerrainHeight_(terrain, rightX0, rightZ0, perpX, perpZ);
        const float leftTerrain1 = SampleBoundaryTerrainHeight_(terrain, leftX1, leftZ1, -perpX, -perpZ);
        const float rightTerrain1 = SampleBoundaryTerrainHeight_(terrain, rightX1, rightZ1, perpX, perpZ);
        const float centerTerrain0 = SampleTerrainHeight_(terrain, x0, z0);
        const float centerTerrain1 = SampleTerrainHeight_(terrain, x1, z1);

        const float leftY0 = CalculateApproachHeight_(leftTerrain0, bridgeHeight, 1.0f - t0, approachLength);
        const float rightY0 = CalculateApproachHeight_(rightTerrain0, bridgeHeight, 1.0f - t0, approachLength);
        const float leftY1 = CalculateApproachHeight_(leftTerrain1, bridgeHeight, 1.0f - t1, approachLength);
        const float rightY1 = CalculateApproachHeight_(rightTerrain1, bridgeHeight, 1.0f - t1, approachLength);
        const float centerY0 = (leftY0 + rightY0) * 0.5f;
        const float centerY1 = (leftY1 + rightY1) * 0.5f;

        const float leftToeY0 = SampleTerrainHeight_(terrain, leftToeX0, leftToeZ0);
        const float rightToeY0 = SampleTerrainHeight_(terrain, rightToeX0, rightToeZ0);
        const float leftToeY1 = SampleTerrainHeight_(terrain, leftToeX1, leftToeZ1);
        const float rightToeY1 = SampleTerrainHeight_(terrain, rightToeX1, rightToeZ1);

        const auto lineColor = [&](const float a, const float b) {
            if (forceInvalid) {
                return kInvalidColor;
            }
            return NodeColorForDelta((a + b) * 0.5f);
        };

        const DWORD centerColor = lineColor(centerY0 - centerTerrain0, centerY1 - centerTerrain1);
        const DWORD leftColor = lineColor(leftY0 - leftTerrain0, leftY1 - leftTerrain1);
        const DWORD rightColor = lineColor(rightY0 - rightTerrain0, rightY1 - rightTerrain1);
        const DWORD leftToeColor = lineColor(leftY0 - leftToeY0, leftY1 - leftToeY1);
        const DWORD rightToeColor = lineColor(rightY0 - rightToeY0, rightY1 - rightToeY1);

        EmitLine({x0, centerY0 + kOverlayHeightOffset, z0, centerColor},
                 {x1, centerY1 + kOverlayHeightOffset, z1, centerColor},
                 kRailThickness, centerColor, layerId);
        EmitLine({leftX0, leftY0 + kOverlayHeightOffset, leftZ0, leftColor},
                 {leftX1, leftY1 + kOverlayHeightOffset, leftZ1, leftColor},
                 kRailThickness, leftColor, layerId);
        EmitLine({rightX0, rightY0 + kOverlayHeightOffset, rightZ0, rightColor},
                 {rightX1, rightY1 + kOverlayHeightOffset, rightZ1, rightColor},
                 kRailThickness, rightColor, layerId);
        EmitLine({leftToeX0, leftToeY0 + kOverlayHeightOffset, leftToeZ0, leftToeColor},
                 {leftToeX1, leftToeY1 + kOverlayHeightOffset, leftToeZ1, leftToeColor},
                 kRailThickness, leftToeColor, layerId);
        EmitLine({rightToeX0, rightToeY0 + kOverlayHeightOffset, rightToeZ0, rightToeColor},
                 {rightToeX1, rightToeY1 + kOverlayHeightOffset, rightToeZ1, rightToeColor},
                 kRailThickness, rightToeColor, layerId);

        if ((i % 4) == 0 || i == (steps - 1)) {
            const DWORD rungColor = forceInvalid
                ? kInvalidColor
                : NodeColorForDelta(((leftY0 - leftTerrain0) + (rightY0 - rightTerrain0)) * 0.5f);
            EmitLine({leftX0, leftY0 + kOverlayHeightOffset, leftZ0, rungColor},
                     {rightX0, rightY0 + kOverlayHeightOffset, rightZ0, rungColor},
                     kRungThickness, rungColor, layerId);
        }

        if ((i % 2) == 0 || i == (steps - 1)) {
            const DWORD leftStrutColor = forceInvalid
                ? kInvalidColor
                : NodeColorForDelta(leftY0 - leftToeY0);
            const DWORD rightStrutColor = forceInvalid
                ? kInvalidColor
                : NodeColorForDelta(rightY0 - rightToeY0);

            EmitLine({leftX0, leftY0 + kOverlayHeightOffset, leftZ0, leftStrutColor},
                     {leftToeX0, leftToeY0 + kOverlayHeightOffset, leftToeZ0, leftStrutColor},
                     kSideStrutThickness, leftStrutColor, layerId);
            EmitLine({rightX0, rightY0 + kOverlayHeightOffset, rightZ0, rightStrutColor},
                     {rightToeX0, rightToeY0 + kOverlayHeightOffset, rightToeZ0, rightStrutColor},
                     kSideStrutThickness, rightStrutColor, layerId);
        }
    }
}

void BridgeApproachRenderer::EmitNodeMarker_(
    const float worldX,
    const float worldZ,
    const float currentHeight,
    const float predictedHeight,
    const DWORD color) {
    if (std::abs(predictedHeight - currentHeight) > 0.05f) {
        EmitLine(
            {worldX, currentHeight + 0.03f, worldZ, color},
            {worldX, predictedHeight + kOverlayHeightOffset, worldZ, color},
            kMarkerThickness,
            color,
            kLayerMarkers);
    }

    EmitLine(
        {worldX - kNodeCrossSize, predictedHeight + kOverlayHeightOffset, worldZ, color},
        {worldX + kNodeCrossSize, predictedHeight + kOverlayHeightOffset, worldZ, color},
        kMarkerThickness,
        color,
        kLayerMarkers);
    EmitLine(
        {worldX, predictedHeight + kOverlayHeightOffset, worldZ - kNodeCrossSize, color},
        {worldX, predictedHeight + kOverlayHeightOffset, worldZ + kNodeCrossSize, color},
        kMarkerThickness,
        color,
        kLayerMarkers);
}

void BridgeApproachRenderer::BuildSingleHeightMarkers_(
    cISTETerrain* terrain,
    const float bridgeEndX, const float bridgeEndZ,
    const float dirX, const float dirZ,
    const float bridgeHeight,
    const float approachLength) {
    const int steps = static_cast<int>(approachLength * 4);
    if (steps <= 0) return;

    for (int i = 0; i <= steps; i += 4) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float dist = approachLength * t * 16.0f;
        const float x = bridgeEndX + dirX * dist;
        const float z = bridgeEndZ + dirZ * dist;
        const float terrainY = SampleTerrainHeight_(terrain, x, z);
        const float approachY = CalculateApproachHeight_(terrainY, bridgeHeight, 1.0f - t, approachLength);
        EmitNodeMarker_(x, z, terrainY, approachY, NodeColorForDelta(approachY - terrainY));
    }
}

float BridgeApproachRenderer::SampleTerrainHeight_(
    cISTETerrain* terrain,
    const float worldX,
    const float worldZ) {
    if (!terrain) return 0.0f;
    return terrain->GetAltitudeAtNearestGrid(worldX, worldZ);
}

float BridgeApproachRenderer::SampleBoundaryTerrainHeight_(
    cISTETerrain* terrain,
    const float worldX,
    const float worldZ,
    const float outwardDirX,
    const float outwardDirZ) {
    const float edgeSample = SampleTerrainHeight_(terrain, worldX, worldZ);
    const float outsideSample = SampleTerrainHeight_(
        terrain,
        worldX + outwardDirX * kOutsideTileOffset,
        worldZ + outwardDirZ * kOutsideTileOffset);
    return (edgeSample + outsideSample) * 0.5f;
}

float BridgeApproachRenderer::CalculateApproachHeight_(
    const float terrainHeight,
    const float bridgeHeight,
    const float t,
    float) {
    return terrainHeight + (bridgeHeight - terrainHeight) * t;
}
