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

// Distance from the core slab, zero inside it. The core is the full requested
// band: it always grades to the plane at full strength. Everything softer lives
// in a skirt of falloffTiles *outside* the core, so the band stays usable and
// the blend into surrounding terrain happens beyond it.
float DistanceOutsideCore(
    const float along,
    const float perpendicular,
    const float length,
    const float lowerBound,
    const float upperBound) noexcept {
    const float alongOver = std::max({
        0.0f,
        -0.5f + kSlabEpsilon - along,
        along - length - 0.5f + kSlabEpsilon});
    const float perpOver = std::max({
        0.0f,
        lowerBound + kSlabEpsilon - perpendicular,
        perpendicular - upperBound + kSlabEpsilon});

    return std::sqrt(alongOver * alongOver + perpOver * perpOver);
}

float ComputeInfluence(const float distanceOutsideCore, const float falloffTiles) noexcept {
    if (distanceOutsideCore <= 0.0f) {
        return 1.0f;
    }
    if (falloffTiles <= 0.0f || distanceOutsideCore >= falloffTiles) {
        return kUnselected;
    }

    const float t = 1.0f - distanceOutsideCore / falloffTiles;
    return t * t * (3.0f - 2.0f * t);
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

bool ConstantGradePreview::IsCoreTile(const int tileX, const int tileZ) const noexcept {
    return InfluenceAtTile(tileX, tileZ) >= 1.0f;
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
        .falloffTiles = std::max(0.0f, request.falloffTiles),
        .requestedWidthTiles = requestedWidthTiles,
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

    const int pad = static_cast<int>(std::ceil(static_cast<float>(path->requestedWidthTiles) / 2.0f))
        + static_cast<int>(std::ceil(path->falloffTiles))
        + 1;
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

            const float influence = ComputeInfluence(
                DistanceOutsideCore(along, perpendicular, path->length, lowerBound, upperBound),
                path->falloffTiles);
            if (influence < 0.0f) continue;

            scanInfluence[Index_(tileX - scanMinX, tileZ - scanMinZ, scanSpanX)] = influence;
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
        .falloffTiles = path->falloffTiles,
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

    const auto vertexAlong = [&](const int vertexX, const int vertexZ) {
        return static_cast<float>(vertexX - request.startTileX) * path->unitX
            + static_cast<float>(vertexZ - request.startTileZ) * path->unitZ;
    };

    // Past the ends of the core the plane would keep climbing, so the skirt
    // blends toward the height at the nearest core edge instead. The bounds are
    // measured rather than derived because the core's vertex extent along the
    // path depends on the drag direction and angle.
    float coreAlongMin = std::numeric_limits<float>::max();
    float coreAlongMax = std::numeric_limits<float>::lowest();
    for (int offsetZ = 0; offsetZ < vertexSpanZ; ++offsetZ) {
        for (int offsetX = 0; offsetX < preview.vertexSpanX; ++offsetX) {
            if (vertexInfluence[Index_(offsetX, offsetZ, preview.vertexSpanX)] < 1.0f) {
                continue;
            }

            const float along = vertexAlong(preview.minVertexX + offsetX, preview.minVertexZ + offsetZ);
            coreAlongMin = std::min(coreAlongMin, along);
            coreAlongMax = std::max(coreAlongMax, along);
        }
    }
    if (coreAlongMin > coreAlongMax) {
        coreAlongMin = 0.0f;
        coreAlongMax = path->length;
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
            const float alongTiles = std::clamp(
                vertexAlong(vertexX, vertexZ), coreAlongMin, coreAlongMax);
            const float planeHeight = path->startHeight + path->gradePerTile * alongTiles;
            const float currentHeight = terrain_->GetAltitudeAtVertex(vertexX, vertexZ);

            preview.vertexIndex[index] = static_cast<int32_t>(preview.vertices.size());
            preview.vertices.push_back(ConstantGradePreview::VertexDelta{
                .vertexX = vertexX,
                .vertexZ = vertexZ,
                .currentHeight = currentHeight,
                .predictedHeight = Lerp(currentHeight, planeHeight, influence),
                .influence = influence,
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
