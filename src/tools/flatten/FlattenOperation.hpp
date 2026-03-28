#pragma once

#include <optional>
#include <vector>

#include "tools/TerrainOperator.hpp"

enum class FlattenHeightMode : uint32_t {
    Explicit = 0,
    Average,
    ReferenceTileAverage,
    Minimum,
    Maximum,
    Delta
};

struct FlattenRequest {
    int x1{};
    int z1{};
    int x2{};
    int z2{};
    int referenceTileX{};
    int referenceTileZ{};
    FlattenHeightMode mode{FlattenHeightMode::Explicit};
    float explicitHeight{250.0f};
    float deltaHeight{7.5f};
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
    float averageHeight{};
    float minimumHeight{};
    float maximumHeight{};
    bool isRectangle{};
    std::vector<VertexDelta> vertices{};

    [[nodiscard]] int CenterTileX() const noexcept { return minTileX + (maxTileX - minTileX) / 2; }
    [[nodiscard]] int CenterTileZ() const noexcept { return minTileZ + (maxTileZ - minTileZ) / 2; }
    [[nodiscard]] const VertexDelta* FindVertex(int vertexX, int vertexZ) const noexcept;
};

class FlattenOperation : public TerrainOperator {
public:
    explicit FlattenOperation(cISTETerrain* terrain) : TerrainOperator(terrain) {}

    [[nodiscard]] std::optional<FlattenPreview> BuildPreview(const FlattenRequest& request) const;
    bool Apply(const FlattenRequest& request);

    static const char* ModeName(FlattenHeightMode mode) noexcept;

private:
    [[nodiscard]] bool IsInBounds_(int tileX, int tileZ) const noexcept;
    [[nodiscard]] FlattenPreview SamplePreview_(int minX, int minZ, int maxX, int maxZ) const;
    [[nodiscard]] float PredictVertexHeight_(int vertexX, int vertexZ, float currentHeight, const FlattenPreview& preview) const noexcept;
};
