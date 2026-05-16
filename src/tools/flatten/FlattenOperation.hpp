#pragma once

#include <optional>
#include <vector>

#include "SC4CellRegion.h"
#include "tools/TerrainOperator.hpp"

enum class FlattenHeightMode : uint32_t {
    Explicit = 0,
    Average,
    ReferenceTileAverage,
    Minimum,
    Maximum,
    Delta
};

enum class FlattenShapeMode : uint32_t {
    Rectangle = 0,
    LineMask
};

struct FlattenRequest {
    int x1{};
    int z1{};
    int x2{};
    int z2{};
    int referenceTileX{};
    int referenceTileZ{};
    FlattenHeightMode mode{FlattenHeightMode::Explicit};
    FlattenShapeMode shape{FlattenShapeMode::Rectangle};
    float explicitHeight{250.0f};
    float deltaHeight{7.5f};
    int32_t lineThickness{1};
};

struct FlattenPreview {
    struct VertexDelta {
        int vertexX{};
        int vertexZ{};
        float currentHeight{};
        float predictedHeight{};

        [[nodiscard]] float Delta() const noexcept { return predictedHeight - currentHeight; }
    };

    int minTileX{};
    int minTileZ{};
    int maxTileX{};
    int maxTileZ{};
    int referenceTileX{};
    int referenceTileZ{};
    int affectedMinTileX{};
    int affectedMinTileZ{};
    int affectedMaxTileX{};
    int affectedMaxTileZ{};
    float targetHeight{};
    float deltaHeight{};
    FlattenHeightMode mode{FlattenHeightMode::Explicit};
    FlattenShapeMode shape{FlattenShapeMode::Rectangle};
    float averageHeight{};
    float minimumHeight{};
    float maximumHeight{};
    int32_t lineThickness{1};
    bool isRectangle{};
    std::optional<SC4CellRegion<int32_t>> selectedTiles{};
    std::vector<VertexDelta> vertices{};

    [[nodiscard]] int CenterTileX() const noexcept { return minTileX + (maxTileX - minTileX) / 2; }
    [[nodiscard]] int CenterTileZ() const noexcept { return minTileZ + (maxTileZ - minTileZ) / 2; }
    [[nodiscard]] const VertexDelta* FindVertex(int vertexX, int vertexZ) const noexcept;
    [[nodiscard]] bool IsSelectedTile(int tileX, int tileZ) const noexcept;
};

class FlattenOperation : public TerrainOperator {
public:
    explicit FlattenOperation(cISTETerrain* terrain) : TerrainOperator(terrain) {}

    [[nodiscard]] std::optional<FlattenPreview> BuildPreview(const FlattenRequest& request) const;
    bool Apply(const FlattenRequest& request);

    static const char* ModeName(FlattenHeightMode mode) noexcept;
    static const char* ShapeName(FlattenShapeMode shape) noexcept;

private:
    [[nodiscard]] bool IsInBounds_(int tileX, int tileZ) const noexcept;
    [[nodiscard]] std::optional<SC4CellRegion<int32_t>> BuildTileSelection_(
        const FlattenRequest& request,
        int minX,
        int minZ,
        int maxX,
        int maxZ) const;
    [[nodiscard]] SC4CellRegion<int32_t> BuildVertexSelection_(const SC4CellRegion<int32_t>& selectedTiles) const;
    [[nodiscard]] FlattenPreview SamplePreview_(
        const SC4CellRegion<int32_t>& selectedTiles,
        const SC4CellRegion<int32_t>& selectedVertices) const;
    [[nodiscard]] float PredictVertexHeight_(int vertexX, int vertexZ, float currentHeight, const FlattenPreview& preview) const noexcept;
};
