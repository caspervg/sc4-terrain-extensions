#include "FlattenRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "cISTETerrain.h"

namespace {
constexpr float kTileSize = 16.0f;
constexpr float kGroundHeightOffset = 0.05f;
constexpr float kHoverHeightOffset = 0.09f;
constexpr float kOverlayHeightOffset = 0.20f;
constexpr float kHoverThickness = 2.5f;
constexpr float kReferenceThickness = 4.0f;
constexpr float kReferenceGuideThickness = 2.0f;
constexpr float kMarkerThickness = 2.0f;
constexpr float kOutlineThickness = 3.0f;
constexpr float kRailThickness = 1.5f;
constexpr float kNodeCrossSize = 1.55f;
constexpr uint32_t kGroundColor = 0x4A9A9A9Au;
constexpr uint32_t kHoverColor = 0xD0101010u;
constexpr uint32_t kReferenceColor = 0xF0D89A28u;
constexpr uint32_t kReferenceFillColor = 0xA0D89A28u;

uint32_t ColorForDelta(const float delta) {
    if (delta > 0.25f) {
        return 0xD030C050;
    }
    if (delta < -0.25f) {
        return 0xD0C84A4A;
    }
    return 0xD0D0D0D0;
}

uint32_t NodeColorForDelta(const float delta) {
    const float magnitude = std::min(std::abs(delta), 20.0f) / 20.0f;
    const uint8_t alpha = static_cast<uint8_t>(128 + magnitude * 110.0f);

    if (delta > 0.05f) {
        const uint8_t green = static_cast<uint8_t>(165 + magnitude * 70.0f);
        return (static_cast<uint32_t>(alpha) << 24)
            | (0x20u << 16)
            | (static_cast<uint32_t>(green) << 8)
            | 0x48u;
    }

    if (delta < -0.05f) {
        const uint8_t red = static_cast<uint8_t>(185 + magnitude * 55.0f);
        return (static_cast<uint32_t>(alpha) << 24)
            | (static_cast<uint32_t>(red) << 16)
            | (0x48u << 8)
            | 0x48u;
    }

    return 0x70C8C8C8u;
}

float WorldXFromVertex(const int vertexX) {
    return static_cast<float>(vertexX) * kTileSize;
}

float WorldZFromVertex(const int vertexZ) {
    return static_cast<float>(vertexZ) * kTileSize;
}
}

void FlattenRenderer::Update(cISTETerrain* terrain, const FlattenPreview& preview) {
    if (!terrain) {
        ClearAll();
        return;
    }

    ClearLayer(kLayerHover);
    const float delta = preview.targetHeight - preview.averageHeight;
    const uint32_t color = ColorForDelta(delta);

    ClearAll();
    BuildGround_(preview);
    BuildFill_(preview, color);
    BuildOutline_(preview, color);
    BuildMarkers_(terrain, preview, color);
    BuildReferenceTile_(terrain, preview);
}

namespace {
void EmitTileHoverOutline(
    FlattenRenderer& renderer,
    cISTETerrain* terrain,
    const int tileX,
    const int tileZ,
    const uint32_t color,
    const float thickness) {
    const OverlayVertex a{WorldXFromVertex(tileX), terrain->GetAltitudeAtVertex(tileX, tileZ) + kHoverHeightOffset, WorldZFromVertex(tileZ), color};
    const OverlayVertex b{WorldXFromVertex(tileX + 1), terrain->GetAltitudeAtVertex(tileX + 1, tileZ) + kHoverHeightOffset, WorldZFromVertex(tileZ), color};
    const OverlayVertex c{WorldXFromVertex(tileX + 1), terrain->GetAltitudeAtVertex(tileX + 1, tileZ + 1) + kHoverHeightOffset, WorldZFromVertex(tileZ + 1), color};
    const OverlayVertex d{WorldXFromVertex(tileX), terrain->GetAltitudeAtVertex(tileX, tileZ + 1) + kHoverHeightOffset, WorldZFromVertex(tileZ + 1), color};

    renderer.EmitScreenLine(a, b, thickness, color, FlattenRenderer::kLayerHover);
    renderer.EmitScreenLine(b, c, thickness, color, FlattenRenderer::kLayerHover);
    renderer.EmitScreenLine(c, d, thickness, color, FlattenRenderer::kLayerHover);
    renderer.EmitScreenLine(d, a, thickness, color, FlattenRenderer::kLayerHover);
}
}

void FlattenRenderer::ShowHoverTile(cISTETerrain* terrain, const int tileX, const int tileZ) {
    ClearLayer(kLayerHover);
    if (!terrain) {
        return;
    }

    EmitTileHoverOutline(*this, terrain, tileX, tileZ, kHoverColor, kHoverThickness);
}

void FlattenRenderer::ShowReferenceHoverTile(cISTETerrain* terrain, const int tileX, const int tileZ) {
    ClearLayer(kLayerHover);
    if (!terrain) {
        return;
    }

    EmitTileHoverOutline(*this, terrain, tileX, tileZ, kReferenceColor, kReferenceThickness);
}

void FlattenRenderer::ClearHoverTile() {
    ClearLayer(kLayerHover);
}

void FlattenRenderer::ClearAll() {
    ClearLayer(kLayerGround);
    ClearLayer(kLayerFill);
    ClearLayer(kLayerOutline);
    ClearLayer(kLayerMarkers);
    ClearLayer(kLayerReference);
    ClearLayer(kLayerHover);
}

void FlattenRenderer::BuildGround_(const FlattenPreview& preview) {
    for (int tileZ = preview.affectedMinTileZ; tileZ <= preview.affectedMaxTileZ; ++tileZ) {
        for (int tileX = preview.affectedMinTileX; tileX <= preview.affectedMaxTileX; ++tileX) {
            if (!preview.IsSelectedTile(tileX, tileZ)) {
                continue;
            }

            const auto* v00 = preview.FindVertex(tileX, tileZ);
            const auto* v10 = preview.FindVertex(tileX + 1, tileZ);
            const auto* v01 = preview.FindVertex(tileX, tileZ + 1);
            const auto* v11 = preview.FindVertex(tileX + 1, tileZ + 1);
            if (!v00 || !v10 || !v01 || !v11) {
                continue;
            }

            const OverlayVertex p00{WorldXFromVertex(v00->vertexX), v00->currentHeight + kGroundHeightOffset, WorldZFromVertex(v00->vertexZ), kGroundColor};
            const OverlayVertex p10{WorldXFromVertex(v10->vertexX), v10->currentHeight + kGroundHeightOffset, WorldZFromVertex(v10->vertexZ), kGroundColor};
            const OverlayVertex p01{WorldXFromVertex(v01->vertexX), v01->currentHeight + kGroundHeightOffset, WorldZFromVertex(v01->vertexZ), kGroundColor};
            const OverlayVertex p11{WorldXFromVertex(v11->vertexX), v11->currentHeight + kGroundHeightOffset, WorldZFromVertex(v11->vertexZ), kGroundColor};

            EmitQuad(p00, p10, p11, p01, kGroundColor, kLayerGround);
        }
    }
}

void FlattenRenderer::BuildFill_(const FlattenPreview& preview, const uint32_t color) {
    for (int tileZ = preview.affectedMinTileZ; tileZ <= preview.affectedMaxTileZ; ++tileZ) {
        for (int tileX = preview.affectedMinTileX; tileX <= preview.affectedMaxTileX; ++tileX) {
            if (!preview.IsSelectedTile(tileX, tileZ)) {
                continue;
            }

            const auto* v00 = preview.FindVertex(tileX, tileZ);
            const auto* v10 = preview.FindVertex(tileX + 1, tileZ);
            const auto* v01 = preview.FindVertex(tileX, tileZ + 1);
            const auto* v11 = preview.FindVertex(tileX + 1, tileZ + 1);
            if (!v00 || !v10 || !v01 || !v11) {
                continue;
            }

            const uint32_t edgeColor = NodeColorForDelta(
                (v00->Delta() + v10->Delta() + v01->Delta() + v11->Delta()) * 0.25f);

            const OverlayVertex p00{WorldXFromVertex(v00->vertexX), v00->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v00->vertexZ), edgeColor};
            const OverlayVertex p10{WorldXFromVertex(v10->vertexX), v10->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v10->vertexZ), edgeColor};
            const OverlayVertex p01{WorldXFromVertex(v01->vertexX), v01->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v01->vertexZ), edgeColor};
            const OverlayVertex p11{WorldXFromVertex(v11->vertexX), v11->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v11->vertexZ), edgeColor};

            EmitScreenLine(p00, p10, kRailThickness, edgeColor, kLayerFill);
            EmitScreenLine(p10, p11, kRailThickness, edgeColor, kLayerFill);
            EmitScreenLine(p11, p01, kRailThickness, edgeColor, kLayerFill);
            EmitScreenLine(p01, p00, kRailThickness, edgeColor, kLayerFill);
        }
    }
}

void FlattenRenderer::BuildOutline_(const FlattenPreview& preview, const uint32_t color) {
    for (int tileZ = preview.minTileZ; tileZ <= preview.maxTileZ; ++tileZ) {
        for (int tileX = preview.minTileX; tileX <= preview.maxTileX; ++tileX) {
            if (!preview.IsSelectedTile(tileX, tileZ)) {
                continue;
            }

            const auto* v00 = preview.FindVertex(tileX, tileZ);
            const auto* v10 = preview.FindVertex(tileX + 1, tileZ);
            const auto* v01 = preview.FindVertex(tileX, tileZ + 1);
            const auto* v11 = preview.FindVertex(tileX + 1, tileZ + 1);
            if (!v00 || !v10 || !v01 || !v11) {
                continue;
            }

            const OverlayVertex a{
                WorldXFromVertex(v00->vertexX),
                v00->predictedHeight + kOverlayHeightOffset + 0.02f,
                WorldZFromVertex(v00->vertexZ),
                color
            };
            const OverlayVertex b{
                WorldXFromVertex(v10->vertexX),
                v10->predictedHeight + kOverlayHeightOffset + 0.02f,
                WorldZFromVertex(v10->vertexZ),
                color
            };
            const OverlayVertex c{
                WorldXFromVertex(v11->vertexX),
                v11->predictedHeight + kOverlayHeightOffset + 0.02f,
                WorldZFromVertex(v11->vertexZ),
                color
            };
            const OverlayVertex d{
                WorldXFromVertex(v01->vertexX),
                v01->predictedHeight + kOverlayHeightOffset + 0.02f,
                WorldZFromVertex(v01->vertexZ),
                color
            };

            if (!preview.IsSelectedTile(tileX, tileZ - 1)) {
                EmitScreenLine(a, b, kOutlineThickness, color, kLayerOutline);
            }
            if (!preview.IsSelectedTile(tileX + 1, tileZ)) {
                EmitScreenLine(b, c, kOutlineThickness, color, kLayerOutline);
            }
            if (!preview.IsSelectedTile(tileX, tileZ + 1)) {
                EmitScreenLine(c, d, kOutlineThickness, color, kLayerOutline);
            }
            if (!preview.IsSelectedTile(tileX - 1, tileZ)) {
                EmitScreenLine(d, a, kOutlineThickness, color, kLayerOutline);
            }
        }
    }
}

void FlattenRenderer::BuildMarkers_(cISTETerrain*, const FlattenPreview& preview, const uint32_t) {
    for (const auto& vertex : preview.vertices) {
        const uint32_t nodeColor = NodeColorForDelta(vertex.Delta());
        const float worldX = WorldXFromVertex(vertex.vertexX);
        const float worldZ = WorldZFromVertex(vertex.vertexZ);
        const float currentHeight = vertex.currentHeight + 0.03f;
        const float predictedHeight = vertex.predictedHeight + kOverlayHeightOffset;

        if (std::abs(vertex.Delta()) > 0.05f) {
            EmitScreenLine(
                {worldX, currentHeight, worldZ, nodeColor},
                {worldX, predictedHeight, worldZ, nodeColor},
                kMarkerThickness,
                nodeColor,
                kLayerMarkers);
        }

        EmitScreenLine(
            {worldX - kNodeCrossSize, predictedHeight, worldZ, nodeColor},
            {worldX + kNodeCrossSize, predictedHeight, worldZ, nodeColor},
            kMarkerThickness,
            nodeColor,
            kLayerMarkers);
        EmitScreenLine(
            {worldX, predictedHeight, worldZ - kNodeCrossSize, nodeColor},
            {worldX, predictedHeight, worldZ + kNodeCrossSize, nodeColor},
            kMarkerThickness,
            nodeColor,
            kLayerMarkers);
    }
}

void FlattenRenderer::BuildReferenceTile_(cISTETerrain* terrain, const FlattenPreview& preview) {
    ClearLayer(kLayerReference);
    if (!terrain || preview.mode != FlattenHeightMode::ReferenceTileAverage) {
        return;
    }

    const int tileX = preview.referenceTileX;
    const int tileZ = preview.referenceTileZ;

    const float planeHeight = preview.targetHeight + kOverlayHeightOffset;

    const OverlayVertex a{WorldXFromVertex(tileX), planeHeight, WorldZFromVertex(tileZ), kReferenceColor};
    const OverlayVertex b{WorldXFromVertex(tileX + 1), planeHeight, WorldZFromVertex(tileZ), kReferenceColor};
    const OverlayVertex c{WorldXFromVertex(tileX + 1), planeHeight, WorldZFromVertex(tileZ + 1), kReferenceColor};
    const OverlayVertex d{WorldXFromVertex(tileX), planeHeight, WorldZFromVertex(tileZ + 1), kReferenceColor};

    EmitQuad(a, b, c, d, kReferenceFillColor, kLayerReference);

    EmitScreenLine(a, b, kReferenceThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(b, c, kReferenceThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(c, d, kReferenceThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(d, a, kReferenceThickness, kReferenceColor, kLayerReference);

    const OverlayVertex groundA{WorldXFromVertex(tileX), terrain->GetAltitudeAtVertex(tileX, tileZ) + 0.03f, WorldZFromVertex(tileZ), kReferenceColor};
    const OverlayVertex groundB{WorldXFromVertex(tileX + 1), terrain->GetAltitudeAtVertex(tileX + 1, tileZ) + 0.03f, WorldZFromVertex(tileZ), kReferenceColor};
    const OverlayVertex groundC{WorldXFromVertex(tileX + 1), terrain->GetAltitudeAtVertex(tileX + 1, tileZ + 1) + 0.03f, WorldZFromVertex(tileZ + 1), kReferenceColor};
    const OverlayVertex groundD{WorldXFromVertex(tileX), terrain->GetAltitudeAtVertex(tileX, tileZ + 1) + 0.03f, WorldZFromVertex(tileZ + 1), kReferenceColor};

    EmitScreenLine(groundA, a, kReferenceGuideThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(groundB, b, kReferenceGuideThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(groundC, c, kReferenceGuideThickness, kReferenceColor, kLayerReference);
    EmitScreenLine(groundD, d, kReferenceGuideThickness, kReferenceColor, kLayerReference);
}
