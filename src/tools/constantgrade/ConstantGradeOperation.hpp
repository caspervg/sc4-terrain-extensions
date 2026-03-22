#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "tools/TerrainOperator.hpp"

struct ConstantGradeRequest {
    int startTileX{};
    int startTileZ{};
    int endTileX{};
    int endTileZ{};
    float widthTiles{1.0f};
    std::optional<float> gradePercent{};
    std::optional<float> startHeight{};
    std::optional<float> endHeight{};
};

struct ConstantGradePreview {
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
    int affectedMinTileX{};
    int affectedMinTileZ{};
    int affectedMaxTileX{};
    int affectedMaxTileZ{};
    bool slopeInX{};
    int pathLengthTiles{};
    int effectiveWidthTiles{};
    float gradePercent{};
    float startHeight{};
    float endHeight{};
    std::vector<VertexDelta> vertices{};

    [[nodiscard]] const VertexDelta* FindVertex(int vertexX, int vertexZ) const noexcept;
};

class ConstantGradeOperation : public TerrainOperator {
public:
    explicit ConstantGradeOperation(cISTETerrain* terrain) : TerrainOperator(terrain) {}

    [[nodiscard]] std::optional<ConstantGradePreview> BuildPreview(const ConstantGradeRequest& request) const;
    bool Apply(const ConstantGradeRequest& request);

private:
    struct PathInfo {
        int tileDx{};
        int tileDz{};
        int pathLengthTiles{};
        bool slopeInX{};
        float stepX{};
        float stepZ{};
        float startHeight{};
        float endHeight{};
        float heightStep{};
        int widthRadius{};
        float gradePercent{};
    };

    [[nodiscard]] std::optional<PathInfo> BuildPathInfo_(const ConstantGradeRequest& request) const;
    [[nodiscard]] bool IsInBounds_(int tileX, int tileZ) const noexcept;

    static int VertexIndex_(int x, int z, int minVertexX, int vertexWidth) noexcept;
    static float LerpHeight_(float currentHeight, float targetHeight, float influence) noexcept;
};
