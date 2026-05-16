#include "FlattenOperation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "SC4Rect.h"

namespace {
constexpr int32_t kDefaultLineThickness = 1;
constexpr int32_t kMaxLineThickness = 9;

int32_t NormalizeLineThickness(int32_t thickness) noexcept {
    thickness = std::clamp(thickness, -kMaxLineThickness, kMaxLineThickness);
    return thickness == 0 ? kDefaultLineThickness : thickness;
}
}

std::optional<FlattenPreview> FlattenOperation::BuildPreview(const FlattenRequest& request) const {
    const int minX = std::min(request.x1, request.x2);
    const int minZ = std::min(request.z1, request.z2);
    const int maxX = std::max(request.x1, request.x2);
    const int maxZ = std::max(request.z1, request.z2);
    const int referenceTileX = IsInBounds_(request.referenceTileX, request.referenceTileZ)
        ? request.referenceTileX
        : request.x1;
    const int referenceTileZ = IsInBounds_(request.referenceTileX, request.referenceTileZ)
        ? request.referenceTileZ
        : request.z1;

    if (!IsInBounds_(minX, minZ) || !IsInBounds_(maxX, maxZ)) {
        return std::nullopt;
    }

    const auto selectedTiles = BuildTileSelection_(request, minX, minZ, maxX, maxZ);
    if (!selectedTiles.has_value()) {
        return std::nullopt;
    }

    const auto selectedVertices = BuildVertexSelection_(*selectedTiles);
    FlattenPreview preview = SamplePreview_(*selectedTiles, selectedVertices);
    preview.selectedTiles = selectedTiles;
    preview.referenceTileX = referenceTileX;
    preview.referenceTileZ = referenceTileZ;
    preview.shape = request.shape;
    preview.lineThickness = NormalizeLineThickness(request.lineThickness);
    preview.isRectangle = request.shape == FlattenShapeMode::Rectangle;

    switch (request.mode) {
    case FlattenHeightMode::Explicit:
        preview.targetHeight = request.explicitHeight;
        break;
    case FlattenHeightMode::Average:
        preview.targetHeight = preview.averageHeight;
        break;
    case FlattenHeightMode::ReferenceTileAverage:
        preview.targetHeight = GetTileAverageHeight(referenceTileX, referenceTileZ);
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
    case FlattenHeightMode::ReferenceTileAverage:
        return "reference avg";
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

const char* FlattenOperation::ShapeName(const FlattenShapeMode shape) noexcept {
    switch (shape) {
    case FlattenShapeMode::Rectangle:
        return "rectangle";
    case FlattenShapeMode::LineMask:
        return "line mask";
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

std::optional<SC4CellRegion<int32_t>> FlattenOperation::BuildTileSelection_(
    const FlattenRequest& request,
    const int minX,
    const int minZ,
    const int maxX,
    const int maxZ) const {
    if (request.shape == FlattenShapeMode::Rectangle) {
        return SC4CellRegion<int32_t>(minX, minZ, maxX, maxZ, true);
    }

    const int thickness = NormalizeLineThickness(request.lineThickness);
    const int expand = std::abs(thickness) - 1;
    const int regionMinX = std::max(0, minX - expand);
    const int regionMinZ = std::max(0, minZ - expand);
    const int regionMaxX = std::min(static_cast<int>(terrain_->CellCountX()) - 1, maxX + expand);
    const int regionMaxZ = std::min(static_cast<int>(terrain_->CellCountZ()) - 1, maxZ + expand);

    SC4CellRegion<int32_t> region(regionMinX, regionMinZ, regionMaxX, regionMaxZ, false);

    const int dx = std::abs(request.x2 - request.x1);
    const int dz = std::abs(request.z2 - request.z1);
    const int sx = request.x1 < request.x2 ? 1 : -1;
    const int sz = request.z1 < request.z2 ? 1 : -1;
    const bool horizontalDominant = dx > dz;
    const int startOffset = thickness > 0 ? 0 : thickness + 1;
    const int endOffset = thickness > 0 ? thickness - 1 : 0;

    int currentX = request.x1;
    int currentZ = request.z1;
    int err = dx - dz;

    while (true) {
        for (int offset = startOffset; offset <= endOffset; ++offset) {
            int tileX = currentX;
            int tileZ = currentZ;

            if (horizontalDominant) {
                tileZ += offset;
            } else {
                tileX += offset;
            }

            if (!IsInBounds_(tileX, tileZ)) {
                continue;
            }

            region.cellMap.SetValue(
                static_cast<uint32_t>(tileX - regionMinX),
                static_cast<uint32_t>(tileZ - regionMinZ),
                true);
        }

        if (currentX == request.x2 && currentZ == request.z2) {
            break;
        }

        const int twiceError = err * 2;
        if (twiceError > -dz) {
            err -= dz;
            currentX += sx;
        }
        if (twiceError < dx) {
            err += dx;
            currentZ += sz;
        }
    }

    return region;
}

SC4CellRegion<int32_t> FlattenOperation::BuildVertexSelection_(const SC4CellRegion<int32_t>& selectedTiles) const {
    const auto& bounds = selectedTiles.bounds;
    SC4CellRegion<int32_t> selectedVertices(
        bounds.topLeftX,
        bounds.topLeftY,
        bounds.bottomRightX + 1,
        bounds.bottomRightY + 1,
        false);

    for (int tileZ = bounds.topLeftY; tileZ <= bounds.bottomRightY; ++tileZ) {
        for (int tileX = bounds.topLeftX; tileX <= bounds.bottomRightX; ++tileX) {
            if (!selectedTiles.cellMap.GetValue(
                static_cast<uint32_t>(tileX - bounds.topLeftX),
                static_cast<uint32_t>(tileZ - bounds.topLeftY))) {
                continue;
            }

            selectedVertices.cellMap.SetValue(
                static_cast<uint32_t>(tileX - bounds.topLeftX),
                static_cast<uint32_t>(tileZ - bounds.topLeftY),
                true);
            selectedVertices.cellMap.SetValue(
                static_cast<uint32_t>(tileX + 1 - bounds.topLeftX),
                static_cast<uint32_t>(tileZ - bounds.topLeftY),
                true);
            selectedVertices.cellMap.SetValue(
                static_cast<uint32_t>(tileX - bounds.topLeftX),
                static_cast<uint32_t>(tileZ + 1 - bounds.topLeftY),
                true);
            selectedVertices.cellMap.SetValue(
                static_cast<uint32_t>(tileX + 1 - bounds.topLeftX),
                static_cast<uint32_t>(tileZ + 1 - bounds.topLeftY),
                true);
        }
    }

    return selectedVertices;
}

FlattenPreview FlattenOperation::SamplePreview_(
    const SC4CellRegion<int32_t>& selectedTiles,
    const SC4CellRegion<int32_t>& selectedVertices) const {
    const auto& tileBounds = selectedTiles.bounds;
    const auto& vertexBounds = selectedVertices.bounds;
    float minimumHeight = std::numeric_limits<float>::max();
    float maximumHeight = std::numeric_limits<float>::lowest();
    float totalHeight = 0.0f;
    int count = 0;

    FlattenPreview preview{
        .minTileX = tileBounds.topLeftX,
        .minTileZ = tileBounds.topLeftY,
        .maxTileX = tileBounds.bottomRightX,
        .maxTileZ = tileBounds.bottomRightY,
        .affectedMinTileX = std::max(0, tileBounds.topLeftX - 1),
        .affectedMinTileZ = std::max(0, tileBounds.topLeftY - 1),
        .affectedMaxTileX = std::min(static_cast<int>(terrain_->CellCountX()) - 1, tileBounds.bottomRightX + 1),
        .affectedMaxTileZ = std::min(static_cast<int>(terrain_->CellCountZ()) - 1, tileBounds.bottomRightY + 1),
        .targetHeight = 0.0f,
        .deltaHeight = 0.0f,
        .mode = FlattenHeightMode::Explicit,
        .shape = FlattenShapeMode::Rectangle,
        .averageHeight = 0.0f,
        .minimumHeight = 0.0f,
        .maximumHeight = 0.0f,
        .lineThickness = kDefaultLineThickness,
        .isRectangle = false
    };

    for (int vertexZ = vertexBounds.topLeftY; vertexZ <= vertexBounds.bottomRightY; ++vertexZ) {
        for (int vertexX = vertexBounds.topLeftX; vertexX <= vertexBounds.bottomRightX; ++vertexX) {
            if (!selectedVertices.cellMap.GetValue(
                static_cast<uint32_t>(vertexX - vertexBounds.topLeftX),
                static_cast<uint32_t>(vertexZ - vertexBounds.topLeftY))) {
                continue;
            }

            const float height = terrain_->GetAltitudeAtVertex(vertexX, vertexZ);
            minimumHeight = std::min(minimumHeight, height);
            maximumHeight = std::max(maximumHeight, height);
            totalHeight += height;
            ++count;

            preview.vertices.push_back(FlattenPreview::VertexDelta{
                .vertexX = vertexX,
                .vertexZ = vertexZ,
                .currentHeight = height,
                .predictedHeight = 0.0f
            });
        }
    }

    const float averageHeight = count > 0 ? totalHeight / static_cast<float>(count) : 0.0f;
    preview.targetHeight = averageHeight;
    preview.averageHeight = averageHeight;
    preview.minimumHeight = count > 0 ? minimumHeight : 0.0f;
    preview.maximumHeight = count > 0 ? maximumHeight : 0.0f;
    preview.isRectangle = false;

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

bool FlattenPreview::IsSelectedTile(const int tileX, const int tileZ) const noexcept {
    if (!selectedTiles.has_value()) {
        return false;
    }

    const auto& bounds = selectedTiles->bounds;
    if (tileX < bounds.topLeftX
        || tileZ < bounds.topLeftY
        || tileX > bounds.bottomRightX
        || tileZ > bounds.bottomRightY) {
        return false;
    }

    return selectedTiles->cellMap.GetValue(
        static_cast<uint32_t>(tileX - bounds.topLeftX),
        static_cast<uint32_t>(tileZ - bounds.topLeftY));
}

float FlattenOperation::PredictVertexHeight_(
    const int vertexX,
    const int vertexZ,
    const float currentHeight,
    const FlattenPreview& preview) const noexcept {
    if (!preview.FindVertex(vertexX, vertexZ)) {
        return currentHeight;
    }

    if (preview.mode == FlattenHeightMode::Delta) {
        return currentHeight + preview.deltaHeight;
    }
    return preview.targetHeight;
}
