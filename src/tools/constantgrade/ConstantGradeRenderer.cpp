#include "ConstantGradeRenderer.hpp"

#include <cmath>

namespace {

constexpr float kTileSize = 16.0f;
constexpr float kOverlayHeightOffset = 0.25f;
constexpr float kOutlineThickness = 1.3f;
constexpr float kMarkerThickness = 0.55f;
constexpr float kCrossSize = 1.25f;

float WorldXFromVertex(const int vertexX) {
    return static_cast<float>(vertexX) * kTileSize;
}

float WorldZFromVertex(const int vertexZ) {
    return static_cast<float>(vertexZ) * kTileSize;
}

DWORD FillColorForDelta(const float delta) {
    if (delta > 0.25f) return 0xA0F0C14Bu;
    if (delta < -0.25f) return 0xA05AA8E6u;
    return 0x90D8D8D8u;
}

DWORD OutlineColorForGrade(const float gradePercent) {
    return std::abs(gradePercent) >= 15.0f ? 0xD0FF7A00u : 0xD0FFD84Au;
}

DWORD MarkerColorForDelta(const float delta) {
    if (delta > 0.1f) return 0xB0FFD04Au;
    if (delta < -0.1f) return 0xB06AB8FFu;
    return 0x80D0D0D0u;
}

}

void ConstantGradeRenderer::Update(cISTETerrain* terrain, const ConstantGradePreview& preview) {
    if (!terrain) {
        ClearAll();
        return;
    }

    ClearAll();
    BuildFill_(preview);
    BuildOutline_(preview);
    BuildMarkers_(preview);
}

void ConstantGradeRenderer::ClearAll() {
    ClearLayer(kLayerFill);
    ClearLayer(kLayerOutline);
    ClearLayer(kLayerMarkers);
}

void ConstantGradeRenderer::BuildFill_(const ConstantGradePreview& preview) {
    for (int tileZ = preview.affectedMinTileZ; tileZ <= preview.affectedMaxTileZ; ++tileZ) {
        for (int tileX = preview.affectedMinTileX; tileX <= preview.affectedMaxTileX; ++tileX) {
            const auto* v00 = preview.FindVertex(tileX, tileZ);
            const auto* v10 = preview.FindVertex(tileX + 1, tileZ);
            const auto* v01 = preview.FindVertex(tileX, tileZ + 1);
            const auto* v11 = preview.FindVertex(tileX + 1, tileZ + 1);
            if (!v00 || !v10 || !v01 || !v11) continue;

            const float avgDelta = (v00->Delta() + v10->Delta() + v01->Delta() + v11->Delta()) * 0.25f;
            const DWORD color = FillColorForDelta(avgDelta);

            EmitQuad(
                {WorldXFromVertex(v00->vertexX), v00->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v00->vertexZ), color},
                {WorldXFromVertex(v10->vertexX), v10->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v10->vertexZ), color},
                {WorldXFromVertex(v11->vertexX), v11->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v11->vertexZ), color},
                {WorldXFromVertex(v01->vertexX), v01->predictedHeight + kOverlayHeightOffset, WorldZFromVertex(v01->vertexZ), color},
                color,
                kLayerFill);
        }
    }
}

void ConstantGradeRenderer::BuildOutline_(const ConstantGradePreview& preview) {
    const DWORD color = OutlineColorForGrade(preview.gradePercent);
    const auto* aV = preview.FindVertex(preview.affectedMinTileX, preview.affectedMinTileZ);
    const auto* bV = preview.FindVertex(preview.affectedMaxTileX + 1, preview.affectedMinTileZ);
    const auto* cV = preview.FindVertex(preview.affectedMaxTileX + 1, preview.affectedMaxTileZ + 1);
    const auto* dV = preview.FindVertex(preview.affectedMinTileX, preview.affectedMaxTileZ + 1);
    if (!aV || !bV || !cV || !dV) return;

    const OverlayVertex a{WorldXFromVertex(aV->vertexX), aV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(aV->vertexZ), color};
    const OverlayVertex b{WorldXFromVertex(bV->vertexX), bV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(bV->vertexZ), color};
    const OverlayVertex c{WorldXFromVertex(cV->vertexX), cV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(cV->vertexZ), color};
    const OverlayVertex d{WorldXFromVertex(dV->vertexX), dV->predictedHeight + kOverlayHeightOffset + 0.02f, WorldZFromVertex(dV->vertexZ), color};

    EmitLine(a, b, kOutlineThickness, color, kLayerOutline);
    EmitLine(b, c, kOutlineThickness, color, kLayerOutline);
    EmitLine(c, d, kOutlineThickness, color, kLayerOutline);
    EmitLine(d, a, kOutlineThickness, color, kLayerOutline);
}

void ConstantGradeRenderer::BuildMarkers_(const ConstantGradePreview& preview) {
    for (const auto& vertex : preview.vertices) {
        const float delta = vertex.Delta();
        if (std::abs(delta) <= 0.05f) continue;

        const DWORD color = MarkerColorForDelta(delta);
        const float worldX = WorldXFromVertex(vertex.vertexX);
        const float worldZ = WorldZFromVertex(vertex.vertexZ);
        const float currentHeight = vertex.currentHeight + 0.03f;
        const float predictedHeight = vertex.predictedHeight + kOverlayHeightOffset;

        EmitLine({worldX, currentHeight, worldZ, color},
                 {worldX, predictedHeight, worldZ, color},
                 kMarkerThickness,
                 color,
                 kLayerMarkers);
        EmitLine({worldX - kCrossSize, predictedHeight, worldZ, color},
                 {worldX + kCrossSize, predictedHeight, worldZ, color},
                 kMarkerThickness,
                 color,
                 kLayerMarkers);
        EmitLine({worldX, predictedHeight, worldZ - kCrossSize, color},
                 {worldX, predictedHeight, worldZ + kCrossSize, color},
                 kMarkerThickness,
                 color,
                 kLayerMarkers);
    }
}
