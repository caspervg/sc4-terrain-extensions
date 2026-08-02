#include "ConstantGradeOperation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "SC4Rect.h"
#include "tools/bridge/BridgeApproachGeometry.hpp"

namespace {

constexpr float kTileSizeMeters = 16.0f;
constexpr float kSlabEpsilon = 1e-4f;
constexpr float kUnselected = -1.0f;

float ComputeInfluence(
    const float perpendicularDistance,
    const int effectiveWidthTiles,
    const bool sideSmoothing) noexcept {
    if (!sideSmoothing || effectiveWidthTiles <= 1) {
        return 1.0f;
    }

    return std::clamp(
        1.0f - std::abs(perpendicularDistance) / (static_cast<float>(effectiveWidthTiles) / 2.0f),
        0.0f,
        1.0f);
}

}

const ConstantGradePreview::VertexDelta* ConstantGradePreview::FindVertex(
    const int vertexX,
    const int vertexZ) const noexcept {
    if (vertexSpanX <= 0) {
        return nullptr;
    }

    const int offsetX = vertexX - minVertexX;
    const int offsetZ = vertexZ - minVertexZ;
    const int spanZ = static_cast<int>(vertexIndex.size()) / vertexSpanX;
    if (offsetX < 0 || offsetX >= vertexSpanX || offsetZ < 0 || offsetZ >= spanZ) {
        return nullptr;
    }

    const int32_t index = vertexIndex[static_cast<size_t>(offsetZ) * vertexSpanX + offsetX];
    return index < 0 ? nullptr : &vertices[static_cast<size_t>(index)];
}

float ConstantGradePreview::InfluenceAtTile(const int tileX, const int tileZ) const noexcept {
    if (tileInfluence.empty()
        || tileX < minTileX || tileX > maxTileX
        || tileZ < minTileZ || tileZ > maxTileZ) {
        return kUnselected;
    }

    const int spanX = maxTileX - minTileX + 1;
    return tileInfluence[static_cast<size_t>(tileZ - minTileZ) * spanX + (tileX - minTileX)];
}

bool ConstantGradePreview::IsSelectedTile(const int tileX, const int tileZ) const noexcept {
    return InfluenceAtTile(tileX, tileZ) >= 0.0f;
}

int ConstantGradeOperation::Index_(const int offsetX, const int offsetZ, const int spanX) noexcept {
    return offsetZ * spanX + offsetX;
}

bool ConstantGradeOperation::IsInBounds_(const int tileX, const int tileZ) const noexcept {
    return tileX >= 0
        && tileZ >= 0
        && static_cast<uint32_t>(tileX) < terrain_->CellCountX()
        && static_cast<uint32_t>(tileZ) < terrain_->CellCountZ();
}

int ConstantGradeOperation::ClampTileX_(const int tileX) const noexcept {
    return std::clamp(tileX, 0, std::max(0, static_cast<int>(terrain_->CellCountX()) - 1));
}

int ConstantGradeOperation::ClampTileZ_(const int tileZ) const noexcept {
    return std::clamp(tileZ, 0, std::max(0, static_cast<int>(terrain_->CellCountZ()) - 1));
}

std::optional<ConstantGradeOperation::PathInfo> ConstantGradeOperation::BuildPathInfo_(
    const ConstantGradeRequest& request) const {
    const int tileDx = request.endTileX - request.startTileX;
    const int tileDz = request.endTileZ - request.startTileZ;
    if (tileDx == 0 && tileDz == 0) {
        return std::nullopt;
    }

    const float length = std::sqrt(static_cast<float>(tileDx * tileDx + tileDz * tileDz));
    const float unitX = static_cast<float>(tileDx) / length;
    const float unitZ = static_cast<float>(tileDz) / length;

    const float startHeight = request.startHeight.value_or(
        GetTileAverageHeight(request.startTileX, request.startTileZ));
    const float horizontalDistanceMeters = length * kTileSizeMeters;
    const float endHeight = request.gradePercent.has_value()
        ? startHeight + (request.gradePercent.value() / 100.0f) * horizontalDistanceMeters
        : request.endHeight.value_or(GetTileAverageHeight(request.endTileX, request.endTileZ));

    const int requestedWidthTiles = BridgeApproachGeometry::GetEffectiveWidthTiles(request.widthTiles);
    const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(requestedWidthTiles);

    int influencedRows = 0;
    for (int offset = -widthOffsets.negativeOffset; offset <= widthOffsets.positiveOffset; ++offset) {
        if (ComputeInfluence(static_cast<float>(offset), requestedWidthTiles, request.sideSmoothing) > 0.0f) {
            ++influencedRows;
        }
    }

    return PathInfo{
        .tileDx = tileDx,
        .tileDz = tileDz,
        .length = length,
        .unitX = unitX,
        .unitZ = unitZ,
        .normalX = -unitZ,
        .normalZ = unitX,
        .startHeight = startHeight,
        .endHeight = endHeight,
        .gradePerTile = (endHeight - startHeight) / length,
        .gradePercent = ((endHeight - startHeight) / horizontalDistanceMeters) * 100.0f,
        .angleDegrees = std::atan2(static_cast<float>(tileDz), static_cast<float>(tileDx))
            * (180.0f / std::numbers::pi_v<float>),
        .requestedWidthTiles = requestedWidthTiles,
        .effectiveWidthTiles = std::max(1, influencedRows),
        .negativeOffset = widthOffsets.negativeOffset,
        .positiveOffset = widthOffsets.positiveOffset,
    };
}

std::optional<ConstantGradePreview> ConstantGradeOperation::BuildPreview(
    const ConstantGradeRequest& request) const {
    if (!IsInBounds_(request.startTileX, request.startTileZ)
        || !IsInBounds_(request.endTileX, request.endTileZ)) {
        return std::nullopt;
    }

    const auto path = BuildPathInfo_(request);
    if (!path.has_value()) {
        return std::nullopt;
    }

    const int pad = static_cast<int>(std::ceil(static_cast<float>(path->requestedWidthTiles) / 2.0f)) + 1;
    const int scanMinX = ClampTileX_(std::min(request.startTileX, request.endTileX) - pad);
    const int scanMaxX = ClampTileX_(std::max(request.startTileX, request.endTileX) + pad);
    const int scanMinZ = ClampTileZ_(std::min(request.startTileZ, request.endTileZ) - pad);
    const int scanMaxZ = ClampTileZ_(std::max(request.startTileZ, request.endTileZ) + pad);
    const int scanSpanX = scanMaxX - scanMinX + 1;
    const int scanSpanZ = scanMaxZ - scanMinZ + 1;

    // The slab is anchored at the start tile's centre while the plane in
    // PlaneHeight is anchored at the start tile's minimum-corner vertex. The
    // split is deliberate: the vertex anchor is what keeps axis-aligned output
    // matching the pre-plane implementation.
    const float lowerBound = -static_cast<float>(path->negativeOffset) - 0.5f;
    const float upperBound = static_cast<float>(path->positiveOffset) + 0.5f;

    std::vector<float> scanInfluence(static_cast<size_t>(scanSpanX) * scanSpanZ, kUnselected);
    int tightMinX = std::numeric_limits<int>::max();
    int tightMinZ = std::numeric_limits<int>::max();
    int tightMaxX = std::numeric_limits<int>::min();
    int tightMaxZ = std::numeric_limits<int>::min();

    for (int tileZ = scanMinZ; tileZ <= scanMaxZ; ++tileZ) {
        for (int tileX = scanMinX; tileX <= scanMaxX; ++tileX) {
            const float centreDx = static_cast<float>(tileX - request.startTileX);
            const float centreDz = static_cast<float>(tileZ - request.startTileZ);
            const float along = centreDx * path->unitX + centreDz * path->unitZ;
            const float perpendicular = centreDx * path->normalX + centreDz * path->normalZ;

            if (along < -0.5f - kSlabEpsilon || along >= path->length + 0.5f - kSlabEpsilon) continue;
            if (perpendicular < lowerBound - kSlabEpsilon) continue;
            if (perpendicular >= upperBound - kSlabEpsilon) continue;

            scanInfluence[Index_(tileX - scanMinX, tileZ - scanMinZ, scanSpanX)] =
                ComputeInfluence(perpendicular, path->requestedWidthTiles, request.sideSmoothing);
            tightMinX = std::min(tightMinX, tileX);
            tightMinZ = std::min(tightMinZ, tileZ);
            tightMaxX = std::max(tightMaxX, tileX);
            tightMaxZ = std::max(tightMaxZ, tileZ);
        }
    }

    if (tightMinX > tightMaxX || tightMinZ > tightMaxZ) {
        return std::nullopt;
    }

    const int tileSpanX = tightMaxX - tightMinX + 1;
    const int tileSpanZ = tightMaxZ - tightMinZ + 1;

    ConstantGradePreview preview{
        .minTileX = tightMinX,
        .minTileZ = tightMinZ,
        .maxTileX = tightMaxX,
        .maxTileZ = tightMaxZ,
        .affectedMinTileX = tightMinX,
        .affectedMinTileZ = tightMinZ,
        .affectedMaxTileX = tightMaxX,
        .affectedMaxTileZ = tightMaxZ,
        .pathLengthTiles = static_cast<int>(std::round(path->length)),
        .requestedWidthTiles = path->requestedWidthTiles,
        .effectiveWidthTiles = path->effectiveWidthTiles,
        .pathAngleDegrees = path->angleDegrees,
        .gradePercent = path->gradePercent,
        .startHeight = path->startHeight,
        .endHeight = path->endHeight,
    };

    preview.tileInfluence.assign(static_cast<size_t>(tileSpanX) * tileSpanZ, kUnselected);
    for (int tileZ = tightMinZ; tileZ <= tightMaxZ; ++tileZ) {
        for (int tileX = tightMinX; tileX <= tightMaxX; ++tileX) {
            preview.tileInfluence[Index_(tileX - tightMinX, tileZ - tightMinZ, tileSpanX)] =
                scanInfluence[Index_(tileX - scanMinX, tileZ - scanMinZ, scanSpanX)];
        }
    }

    preview.minVertexX = tightMinX;
    preview.minVertexZ = tightMinZ;
    preview.vertexSpanX = tileSpanX + 1;
    const int vertexSpanZ = tileSpanZ + 1;

    std::vector<float> vertexInfluence(
        static_cast<size_t>(preview.vertexSpanX) * vertexSpanZ, kUnselected);

    for (int tileZ = tightMinZ; tileZ <= tightMaxZ; ++tileZ) {
        for (int tileX = tightMinX; tileX <= tightMaxX; ++tileX) {
            const float influence = preview.tileInfluence[
                Index_(tileX - tightMinX, tileZ - tightMinZ, tileSpanX)];
            if (influence < 0.0f) {
                continue;
            }

            for (int cornerZ = 0; cornerZ <= 1; ++cornerZ) {
                for (int cornerX = 0; cornerX <= 1; ++cornerX) {
                    const int index = Index_(
                        tileX + cornerX - preview.minVertexX,
                        tileZ + cornerZ - preview.minVertexZ,
                        preview.vertexSpanX);
                    vertexInfluence[index] = std::max(vertexInfluence[index], influence);
                }
            }
        }
    }

    preview.vertexIndex.assign(vertexInfluence.size(), -1);
    preview.vertices.reserve(vertexInfluence.size());

    for (int offsetZ = 0; offsetZ < vertexSpanZ; ++offsetZ) {
        for (int offsetX = 0; offsetX < preview.vertexSpanX; ++offsetX) {
            const int index = Index_(offsetX, offsetZ, preview.vertexSpanX);
            const float influence = vertexInfluence[index];
            if (influence < 0.0f) {
                continue;
            }

            const int vertexX = preview.minVertexX + offsetX;
            const int vertexZ = preview.minVertexZ + offsetZ;
            const float alongTiles =
                static_cast<float>(vertexX - request.startTileX) * path->unitX
                + static_cast<float>(vertexZ - request.startTileZ) * path->unitZ;
            const float planeHeight = path->startHeight + path->gradePerTile * alongTiles;
            const float currentHeight = terrain_->GetAltitudeAtVertex(vertexX, vertexZ);

            preview.vertexIndex[index] = static_cast<int32_t>(preview.vertices.size());
            preview.vertices.push_back(ConstantGradePreview::VertexDelta{
                .vertexX = vertexX,
                .vertexZ = vertexZ,
                .currentHeight = currentHeight,
                .predictedHeight = Lerp(currentHeight, planeHeight, influence),
            });
        }
    }

    return preview;
}

bool ConstantGradeOperation::Apply(const ConstantGradeRequest& request) {
    const auto preview = BuildPreview(request);
    if (!preview.has_value()) {
        return false;
    }

    for (const auto& vertex : preview->vertices) {
        if (std::abs(vertex.Delta()) > 0.001f) {
            SetAltitudeAtVertex(vertex.vertexX, vertex.vertexZ, vertex.predictedHeight);
        }
    }

    Refresh(SC4Rect<int32_t>(
        ClampXToTerrainBounds(preview->minTileX - 1),
        ClampZToTerrainBounds(preview->minTileZ - 1),
        ClampXToTerrainBounds(preview->maxTileX + 2),
        ClampZToTerrainBounds(preview->maxTileZ + 2)));

    return true;
}
