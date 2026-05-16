#include "ConstantGradeOperation.hpp"

#include <algorithm>
#include <cmath>

#include "SC4Rect.h"

namespace {

float ComputeInfluence(const int offset, const float widthTiles, const bool sideSmoothing) noexcept {
    const float distanceFromCenter = static_cast<float>(std::abs(offset));
    if (distanceFromCenter > widthTiles / 2.0f) {
        return -1.0f;
    }

    if (!sideSmoothing || widthTiles <= 1.0f) {
        return 1.0f;
    }

    return std::clamp(
        1.0f - (distanceFromCenter / (widthTiles / 2.0f)),
        0.0f,
        1.0f);
}

}

const ConstantGradePreview::VertexDelta* ConstantGradePreview::FindVertex(
    const int vertexX,
    const int vertexZ) const noexcept {
    for (const auto& vertex : vertices) {
        if (vertex.vertexX == vertexX && vertex.vertexZ == vertexZ) {
            return &vertex;
        }
    }

    return nullptr;
}

std::optional<ConstantGradeOperation::PathInfo> ConstantGradeOperation::BuildPathInfo_(
    const ConstantGradeRequest& request) const {
    const int tileDx = request.endTileX - request.startTileX;
    const int tileDz = request.endTileZ - request.startTileZ;
    const int pathLengthTiles = std::max(std::abs(tileDx), std::abs(tileDz));
    if (pathLengthTiles == 0) {
        return std::nullopt;
    }

    const float startHeight = request.startHeight.value_or(
        GetTileAverageHeight(request.startTileX, request.startTileZ));
    const float horizontalDistanceMeters = std::sqrt(
        static_cast<float>(tileDx * tileDx + tileDz * tileDz)) * 16.0f;
    const float endHeight = request.gradePercent.has_value()
        ? startHeight + (request.gradePercent.value() / 100.0f) * horizontalDistanceMeters
        : request.endHeight.value_or(GetTileAverageHeight(request.endTileX, request.endTileZ));
    const float resolvedGradePercent = horizontalDistanceMeters > 0.0f
        ? ((endHeight - startHeight) / horizontalDistanceMeters) * 100.0f
        : 0.0f;

    return PathInfo{
        .tileDx = tileDx,
        .tileDz = tileDz,
        .pathLengthTiles = pathLengthTiles,
        .slopeInX = std::abs(tileDx) >= std::abs(tileDz),
        .stepX = static_cast<float>(tileDx) / static_cast<float>(pathLengthTiles),
        .stepZ = static_cast<float>(tileDz) / static_cast<float>(pathLengthTiles),
        .startHeight = startHeight,
        .endHeight = endHeight,
        .heightStep = (endHeight - startHeight) / static_cast<float>(pathLengthTiles),
        .widthRadius = static_cast<int>(std::ceil(request.widthTiles / 2.0f)),
        .gradePercent = resolvedGradePercent,
    };
}

bool ConstantGradeOperation::IsInBounds_(const int tileX, const int tileZ) const noexcept {
    return tileX >= 0
        && tileZ >= 0
        && static_cast<uint32_t>(tileX) < terrain_->CellCountX()
        && static_cast<uint32_t>(tileZ) < terrain_->CellCountZ();
}

int ConstantGradeOperation::VertexIndex_(
    const int x,
    const int z,
    const int minVertexX,
    const int vertexWidth) noexcept {
    return z * vertexWidth + (x - minVertexX);
}

float ConstantGradeOperation::LerpHeight_(
    const float currentHeight,
    const float targetHeight,
    const float influence) noexcept {
    return currentHeight + (targetHeight - currentHeight) * influence;
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

    int minTileX = std::min(request.startTileX, request.endTileX);
    int maxTileX = std::max(request.startTileX, request.endTileX);
    int minTileZ = std::min(request.startTileZ, request.endTileZ);
    int maxTileZ = std::max(request.startTileZ, request.endTileZ);

    minTileX = ClampXToTerrainBounds(minTileX - path->widthRadius);
    maxTileX = ClampXToTerrainBounds(maxTileX + path->widthRadius);
    minTileZ = ClampZToTerrainBounds(minTileZ - path->widthRadius);
    maxTileZ = ClampZToTerrainBounds(maxTileZ + path->widthRadius);

    const int minVertexX = minTileX;
    const int maxVertexX = maxTileX + 1;
    const int minVertexZ = minTileZ;
    const int maxVertexZ = maxTileZ + 1;
    const int vertexWidth = maxVertexX - minVertexX + 1;
    const int vertexHeight = maxVertexZ - minVertexZ + 1;

    std::vector<float> predictedHeights(static_cast<size_t>(vertexWidth * vertexHeight));

    ConstantGradePreview preview{
        .minTileX = minTileX,
        .minTileZ = minTileZ,
        .maxTileX = maxTileX,
        .maxTileZ = maxTileZ,
        .affectedMinTileX = minTileX,
        .affectedMinTileZ = minTileZ,
        .affectedMaxTileX = maxTileX,
        .affectedMaxTileZ = maxTileZ,
        .slopeInX = path->slopeInX,
        .pathLengthTiles = path->pathLengthTiles,
        .effectiveWidthTiles = path->widthRadius * 2 + 1,
        .gradePercent = path->gradePercent,
        .startHeight = path->startHeight,
        .endHeight = path->endHeight,
    };
    preview.vertices.reserve(static_cast<size_t>(vertexWidth * vertexHeight));

    for (int z = minVertexZ; z <= maxVertexZ; ++z) {
        for (int x = minVertexX; x <= maxVertexX; ++x) {
            const float currentHeight = terrain_->GetAltitudeAtVertex(x, z);
            predictedHeights[VertexIndex_(x, z - minVertexZ, minVertexX, vertexWidth)] = currentHeight;
            preview.vertices.push_back(ConstantGradePreview::VertexDelta{
                .vertexX = x,
                .vertexZ = z,
                .currentHeight = currentHeight,
                .predictedHeight = currentHeight,
            });
        }
    }

    const int perpDx = path->slopeInX ? 0 : 1;
    const int perpDz = path->slopeInX ? 1 : 0;

    for (int step = 0; step <= path->pathLengthTiles; ++step) {
        const int tileX = static_cast<int>(std::round(request.startTileX + path->stepX * static_cast<float>(step)));
        const int tileZ = static_cast<int>(std::round(request.startTileZ + path->stepZ * static_cast<float>(step)));
        const float currentHeight = path->startHeight + path->heightStep * static_cast<float>(step);

        for (int offset = -path->widthRadius; offset <= path->widthRadius; ++offset) {
            const float influence = ComputeInfluence(offset, request.widthTiles, request.sideSmoothing);
            if (influence < 0.0f) {
                continue;
            }

            const int targetTileX = tileX + perpDx * offset;
            const int targetTileZ = tileZ + perpDz * offset;
            if (!IsValidTile(targetTileX, targetTileZ)) {
                continue;
            }

            const int baseVertexX = targetTileX;
            const int baseVertexZ = targetTileZ;

            const auto applyVertex = [&](const int vertexX, const int vertexZ, const float targetHeight) {
                const int index = VertexIndex_(vertexX, vertexZ - minVertexZ, minVertexX, vertexWidth);
                predictedHeights[index] = LerpHeight_(predictedHeights[index], targetHeight, influence);
            };

            if (path->slopeInX) {
                applyVertex(baseVertexX, baseVertexZ, currentHeight);
                applyVertex(baseVertexX + 1, baseVertexZ, currentHeight + path->heightStep);
                applyVertex(baseVertexX, baseVertexZ + 1, currentHeight);
                applyVertex(baseVertexX + 1, baseVertexZ + 1, currentHeight + path->heightStep);
            }
            else {
                applyVertex(baseVertexX, baseVertexZ, currentHeight);
                applyVertex(baseVertexX + 1, baseVertexZ, currentHeight);
                applyVertex(baseVertexX, baseVertexZ + 1, currentHeight + path->heightStep);
                applyVertex(baseVertexX + 1, baseVertexZ + 1, currentHeight + path->heightStep);
            }
        }
    }

    for (auto& vertex : preview.vertices) {
        vertex.predictedHeight = predictedHeights[
            VertexIndex_(vertex.vertexX, vertex.vertexZ - minVertexZ, minVertexX, vertexWidth)];
    }

    return preview;
}

bool ConstantGradeOperation::Apply(const ConstantGradeRequest& request) {
    const auto path = BuildPathInfo_(request);
    if (!path.has_value()) {
        return false;
    }

    int minTileX = std::min(request.startTileX, request.endTileX) - path->widthRadius;
    int maxTileX = std::max(request.startTileX, request.endTileX) + path->widthRadius;
    int minTileZ = std::min(request.startTileZ, request.endTileZ) - path->widthRadius;
    int maxTileZ = std::max(request.startTileZ, request.endTileZ) + path->widthRadius;

    const int perpDx = path->slopeInX ? 0 : 1;
    const int perpDz = path->slopeInX ? 1 : 0;

    for (int step = 0; step <= path->pathLengthTiles; ++step) {
        const int tileX = static_cast<int>(std::round(request.startTileX + path->stepX * static_cast<float>(step)));
        const int tileZ = static_cast<int>(std::round(request.startTileZ + path->stepZ * static_cast<float>(step)));
        const float currentHeight = path->startHeight + path->heightStep * static_cast<float>(step);

        for (int offset = -path->widthRadius; offset <= path->widthRadius; ++offset) {
            const float influence = ComputeInfluence(offset, request.widthTiles, request.sideSmoothing);
            if (influence < 0.0f) {
                continue;
            }

            const int targetTileX = tileX + perpDx * offset;
            const int targetTileZ = tileZ + perpDz * offset;
            if (!IsValidTile(targetTileX, targetTileZ)) {
                continue;
            }

            Vector3 corners[4];
            GetTileCorners(targetTileX, targetTileZ, corners);

            Vector3 targetCorners[4];
            if (path->slopeInX) {
                targetCorners[0] = Vector3(targetTileX, targetTileZ, currentHeight);
                targetCorners[1] = Vector3(targetTileX + 1, targetTileZ, currentHeight + path->heightStep);
                targetCorners[2] = Vector3(targetTileX, targetTileZ + 1, currentHeight);
                targetCorners[3] = Vector3(targetTileX + 1, targetTileZ + 1, currentHeight + path->heightStep);
            }
            else {
                targetCorners[0] = Vector3(targetTileX, targetTileZ, currentHeight);
                targetCorners[1] = Vector3(targetTileX + 1, targetTileZ, currentHeight);
                targetCorners[2] = Vector3(targetTileX, targetTileZ + 1, currentHeight + path->heightStep);
                targetCorners[3] = Vector3(targetTileX + 1, targetTileZ + 1, currentHeight + path->heightStep);
            }

            for (int i = 0; i < 4; ++i) {
                SetAltitudeAtVertex(
                    static_cast<int>(targetCorners[i].x),
                    static_cast<int>(targetCorners[i].z),
                    Lerp(corners[i].height, targetCorners[i].height, influence));
            }
        }
    }

    Refresh(SC4Rect<int32_t>(
        ClampXToTerrainBounds(minTileX),
        ClampZToTerrainBounds(minTileZ),
        ClampXToTerrainBounds(maxTileX + 1),
        ClampZToTerrainBounds(maxTileZ + 1)));

    return true;
}
