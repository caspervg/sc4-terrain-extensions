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
    float falloffTiles{0.0f};
};

struct ConstantGradePreview {
    struct VertexDelta {
        int vertexX{};
        int vertexZ{};
        float currentHeight{};
        float predictedHeight{};
        float influence{};

        [[nodiscard]] float Delta() const noexcept { return predictedHeight - currentHeight; }
        [[nodiscard]] bool IsCore() const noexcept { return influence >= 1.0f; }
    };

    int minTileX{};
    int minTileZ{};
    int maxTileX{};
    int maxTileZ{};
    int affectedMinTileX{};
    int affectedMinTileZ{};
    int affectedMaxTileX{};
    int affectedMaxTileZ{};
    int pathLengthTiles{};
    int requestedWidthTiles{};
    float falloffTiles{};
    float pathAngleDegrees{};
    float gradePercent{};
    float startHeight{};
    float endHeight{};
    std::vector<VertexDelta> vertices{};

    int minVertexX{};
    int minVertexZ{};
    int vertexSpanX{};
    std::vector<int32_t> vertexIndex{};

    std::vector<float> tileInfluence{};

    [[nodiscard]] const VertexDelta* FindVertex(int vertexX, int vertexZ) const noexcept;
    [[nodiscard]] float InfluenceAtTile(int tileX, int tileZ) const noexcept;
    [[nodiscard]] bool IsSelectedTile(int tileX, int tileZ) const noexcept;
    [[nodiscard]] bool IsCoreTile(int tileX, int tileZ) const noexcept;
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
        float length{};
        float unitX{};
        float unitZ{};
        float normalX{};
        float normalZ{};
        float startHeight{};
        float endHeight{};
        float gradePerTile{};
        float gradePercent{};
        float angleDegrees{};
        float falloffTiles{};
        int requestedWidthTiles{};
        int negativeOffset{};
        int positiveOffset{};
    };

    [[nodiscard]] std::optional<PathInfo> BuildPathInfo_(const ConstantGradeRequest& request) const;
    [[nodiscard]] bool IsInBounds_(int tileX, int tileZ) const noexcept;
    [[nodiscard]] int ClampTileX_(int tileX) const noexcept;
    [[nodiscard]] int ClampTileZ_(int tileZ) const noexcept;

    static int Index_(int offsetX, int offsetZ, int spanX) noexcept;
};
