#include "FlattenRenderer.hpp"

#include <cmath>

#include "cISTETerrain.h"

namespace {
constexpr float kTileSize = 16.0f;
constexpr float kGroundHeightOffset = 0.05f;
constexpr float kOverlayHeightOffset = 0.20f;
constexpr float kMarkerThickness = 0.65f;
constexpr float kOutlineThickness = 1.25f;
constexpr float kRailThickness = 0.50f;
constexpr float kNodeCrossSize = 1.55f;
constexpr DWORD kGroundColor = 0x66D8D8D8u;

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

    const float delta = preview.targetHeight - preview.averageHeight;
    const DWORD color = ColorForDelta(delta);

    ClearAll();
    BuildGround_(preview);
    BuildFill_(preview, color);
    BuildOutline_(preview, color);
    BuildMarkers_(terrain, preview, color);
}

void FlattenRenderer::ClearAll() {
    ClearLayer(kLayerGround);
    ClearLayer(kLayerFill);
    ClearLayer(kLayerOutline);
    ClearLayer(kLayerMarkers);
}

void FlattenRenderer::BuildGround_(const FlattenPreview& preview) {
    for (int tileZ = preview.affectedMinTileZ; tileZ <= preview.affectedMaxTileZ; ++tileZ) {
        for (int tileX = preview.affectedMinTileX; tileX <= preview.affectedMaxTileX; ++tileX) {
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

void FlattenRenderer::BuildFill_(const FlattenPreview& preview, const DWORD color) {
    for (int tileZ = preview.affectedMinTileZ; tileZ <= preview.affectedMaxTileZ; ++tileZ) {
        for (int tileX = preview.affectedMinTileX; tileX <= preview.affectedMaxTileX; ++tileX) {
            const auto* v00 = preview.FindVertex(tileX, tileZ);
            const auto* v10 = preview.FindVertex(tileX + 1, tileZ);
            const auto* v01 = preview.FindVertex(tileX, tileZ + 1);
            const auto* v11 = preview.FindVertex(tileX + 1, tileZ + 1);
            if (!v00 || !v10 || !v01 || !v11) {
                continue;
            }

            const DWORD edgeColor = NodeColorForDelta(
                (v00->Delta() + v10->Delta() + v01->Delta() + v11->Delta()) * 0.25f);

            const OverlayVertex p00{WorldXFromVertex(v00->vertexX), v00->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v00->vertexZ), edgeColor};
            const OverlayVertex p10{WorldXFromVertex(v10->vertexX), v10->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v10->vertexZ), edgeColor};
            const OverlayVertex p01{WorldXFromVertex(v01->vertexX), v01->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v01->vertexZ), edgeColor};
            const OverlayVertex p11{WorldXFromVertex(v11->vertexX), v11->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v11->vertexZ), edgeColor};

            EmitLine(p00, p10, kRailThickness, edgeColor, kLayerFill);
            EmitLine(p10, p11, kRailThickness, edgeColor, kLayerFill);
            EmitLine(p11, p01, kRailThickness, edgeColor, kLayerFill);
            EmitLine(p01, p00, kRailThickness, edgeColor, kLayerFill);
        }
    }
}

void FlattenRenderer::BuildOutline_(const FlattenPreview& preview, const DWORD color) {
    const auto* aV = preview.FindVertex(preview.minTileX, preview.minTileZ);
    const auto* bV = preview.FindVertex(preview.maxTileX + 1, preview.minTileZ);
    const auto* cV = preview.FindVertex(preview.maxTileX + 1, preview.maxTileZ + 1);
    const auto* dV = preview.FindVertex(preview.minTileX, preview.maxTileZ + 1);
    if (!aV || !bV || !cV || !dV) {
        return;
    }

    const OverlayVertex a{WorldXFromVertex(aV->vertexX), aV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(aV->vertexZ), color};
    const OverlayVertex b{WorldXFromVertex(bV->vertexX), bV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(bV->vertexZ), color};
    const OverlayVertex c{WorldXFromVertex(cV->vertexX), cV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(cV->vertexZ), color};
    const OverlayVertex d{WorldXFromVertex(dV->vertexX), dV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(dV->vertexZ), color};

    EmitLine(a, b, kOutlineThickness, color, kLayerOutline);
    EmitLine(b, c, kOutlineThickness, color, kLayerOutline);
    EmitLine(c, d, kOutlineThickness, color, kLayerOutline);
    EmitLine(d, a, kOutlineThickness, color, kLayerOutline);
}

void FlattenRenderer::BuildMarkers_(cISTETerrain*, const FlattenPreview& preview, const DWORD) {
    for (const auto& vertex : preview.vertices) {
        const DWORD nodeColor = NodeColorForDelta(vertex.Delta());
        const float worldX = WorldXFromVertex(vertex.vertexX);
        const float worldZ = WorldZFromVertex(vertex.vertexZ);
        const float currentHeight = vertex.currentHeight + 0.03f;
        const float predictedHeight = vertex.predictedHeight + kOverlayHeightOffset;

        if (std::abs(vertex.Delta()) > 0.05f) {
            EmitLine(
                {worldX, currentHeight, worldZ, nodeColor},
                {worldX, predictedHeight, worldZ, nodeColor},
                kMarkerThickness,
                nodeColor,
                kLayerMarkers);
        }

        EmitLine(
            {worldX - kNodeCrossSize, predictedHeight, worldZ, nodeColor},
            {worldX + kNodeCrossSize, predictedHeight, worldZ, nodeColor},
            kMarkerThickness,
            nodeColor,
            kLayerMarkers);
        EmitLine(
            {worldX, predictedHeight, worldZ - kNodeCrossSize, nodeColor},
            {worldX, predictedHeight, worldZ + kNodeCrossSize, nodeColor},
            kMarkerThickness,
            nodeColor,
            kLayerMarkers);
    }
}
