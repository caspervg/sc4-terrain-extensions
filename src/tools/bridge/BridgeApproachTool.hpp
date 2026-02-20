#pragma once
#include "../TerrainTool.hpp"

class BridgeApproachTool : public TerrainOperator {
public:
    explicit BridgeApproachTool(cISTETerrain* terrain);
    ~BridgeApproachTool() override = default;

    void CreateBridgeApproaches(
        int startTileX, int startTileZ,
        int endTileX, int endTileZ,
        float bridgeHeight,
        float approachLength,
        float maxGrade,
        float widthTiles,
        bool useTapering = false
    );

private:
    static float SampleTileHeight_(void* context, int tileX, int tileZ);

    void CreateSingleApproach_(
        int startTileX, int startTileZ,
        int endTileX, int endTileZ,
        float startHeight,
        float endHeight,
        float widthTiles,
        const char* label,
        bool useTapering = false
    );

    void ApplyGradeToTileWidth_(
        int centerTileX, int centerTileZ,
        float baseHeight,
        float widthTiles,
        bool slopeInX,
        float heightStep,
        bool useTapering = false
    );

    void ApplyBuildableGradeToTile_(
        int tileX, int tileZ,
        float baseHeight,
        float influence,
        bool slopeInX,
        float heightStep
    );
};
