#include "FlattenOperation.hpp"

#include <algorithm>
#include <limits>
#include <cmath>

#include "SC4Rect.h"

std::optional<FlattenPreview> FlattenOperation::BuildPreview(const FlattenRequest& request) const {
    const int minX = std::min(request.x1, request.x2);
    const int minZ = std::min(request.z1, request.z2);
    const int maxX = std::max(request.x1, request.x2);
    const int maxZ = std::max(request.z1, request.z2);

    if (!IsInBounds_(minX, minZ) || !IsInBounds_(maxX, maxZ)) {
        return std::nullopt;
    }

    FlattenPreview preview = SamplePreview_(minX, minZ, maxX, maxZ);

    switch (request.mode) {
    case FlattenHeightMode::Explicit:
        preview.targetHeight = request.explicitHeight;
        break;
    case FlattenHeightMode::Average:
        preview.targetHeight = preview.averageHeight;
        break;
    case FlattenHeightMode::Minimum:
        preview.targetHeight = preview.minimumHeight;
        break;
    case FlattenHeightMode::Maximum:
        preview.targetHeight = preview.maximumHeight;
        break;
    case FlattenHeightMode::Delta:
        preview.targetHeight = preview.averageHeight + request.deltaHeight;
        break;
    }
    preview.mode = request.mode;
    preview.deltaHeight = request.deltaHeight;

    for (auto& vertex : preview.vertices) {
        vertex.predictedHeight = PredictVertexHeight_(
            vertex.vertexX,
            vertex.vertexZ,
            vertex.currentHeight,
            preview);
    }

    return preview;
}

bool FlattenOperation::Apply(const FlattenRequest& request) {
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
        preview->affectedMinTileX,
        preview->affectedMinTileZ,
        preview->affectedMaxTileX + 1,
        preview->affectedMaxTileZ + 1));

    return true;
}

const char* FlattenOperation::ModeName(const FlattenHeightMode mode) noexcept {
    switch (mode) {
    case FlattenHeightMode::Explicit:
        return "explicit";
    case FlattenHeightMode::Average:
        return "average";
    case FlattenHeightMode::Minimum:
        return "minimum";
    case FlattenHeightMode::Maximum:
        return "maximum";
    case FlattenHeightMode::Delta:
        return "delta";
    default:
        return "unknown";
    }
}

bool FlattenOperation::IsInBounds_(const int tileX, const int tileZ) const noexcept {
    return tileX >= 0
        && tileZ >= 0
        && static_cast<uint32_t>(tileX) < terrain_->CellCountX()
        && static_cast<uint32_t>(tileZ) < terrain_->CellCountZ();
}

FlattenPreview FlattenOperation::SamplePreview_(const int minX, const int minZ, const int maxX, const int maxZ) const {
    float minimumHeight = std::numeric_limits<float>::max();
    float maximumHeight = std::numeric_limits<float>::lowest();
    float totalHeight = 0.0f;
    int count = 0;

    for (int z = minZ; z <= maxZ + 1; ++z) {
        for (int x = minX; x <= maxX + 1; ++x) {
            if (!terrain_->LocationIsInBounds(static_cast<float>(x), static_cast<float>(z))) {
                continue;
            }

            const float height = terrain_->GetAltitudeAtVertex(x, z);
            minimumHeight = std::min(minimumHeight, height);
            maximumHeight = std::max(maximumHeight, height);
            totalHeight += height;
            ++count;
        }
    }

    const float averageHeight = count > 0 ? totalHeight / static_cast<float>(count) : 0.0f;

    FlattenPreview preview{
        .minTileX = minX,
        .minTileZ = minZ,
        .maxTileX = maxX,
        .maxTileZ = maxZ,
        .affectedMinTileX = std::max(0, minX - 1),
        .affectedMinTileZ = std::max(0, minZ - 1),
        .affectedMaxTileX = std::min(static_cast<int>(terrain_->CellCountX()) - 1, maxX + 1),
        .affectedMaxTileZ = std::min(static_cast<int>(terrain_->CellCountZ()) - 1, maxZ + 1),
        .targetHeight = averageHeight,
        .deltaHeight = 0.0f,
        .mode = FlattenHeightMode::Explicit,
        .averageHeight = averageHeight,
        .minimumHeight = count > 0 ? minimumHeight : 0.0f,
        .maximumHeight = count > 0 ? maximumHeight : 0.0f,
        .isRectangle = minX != maxX || minZ != maxZ
    };

    preview.vertices.reserve(
        static_cast<size_t>(preview.affectedMaxTileX - preview.affectedMinTileX + 2)
        * static_cast<size_t>(preview.affectedMaxTileZ - preview.affectedMinTileZ + 2));

    for (int z = preview.affectedMinTileZ; z <= preview.affectedMaxTileZ + 1; ++z) {
        for (int x = preview.affectedMinTileX; x <= preview.affectedMaxTileX + 1; ++x) {
            preview.vertices.push_back(FlattenPreview::VertexDelta{
                .vertexX = x,
                .vertexZ = z,
                .currentHeight = terrain_->GetAltitudeAtVertex(x, z),
                .predictedHeight = 0.0f
            });
        }
    }

    return preview;
}

const FlattenPreview::VertexDelta* FlattenPreview::FindVertex(const int vertexX, const int vertexZ) const noexcept {
    for (const auto& vertex : vertices) {
        if (vertex.vertexX == vertexX && vertex.vertexZ == vertexZ) {
            return &vertex;
        }
    }
    return nullptr;
}

float FlattenOperation::PredictVertexHeight_(
    const int vertexX,
    const int vertexZ,
    const float currentHeight,
    const FlattenPreview& preview) const noexcept {
    if (vertexX >= preview.minTileX
        && vertexX <= preview.maxTileX + 1
        && vertexZ >= preview.minTileZ
        && vertexZ <= preview.maxTileZ + 1) {
        if (preview.mode == FlattenHeightMode::Delta) {
            return currentHeight + preview.deltaHeight;
        }
        return preview.targetHeight;
    }

    return currentHeight;
}
