#pragma once
#include "../TerrainTool.hpp"
#include "BridgeApproachGeometry.hpp"

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
        bool useTapering = false,
        BridgeApproachGeometry::ApproachSideMode sideMode = BridgeApproachGeometry::ApproachSideMode::Both
    );

private:
    static float SampleTileHeight_(void* context, int tileX, int tileZ);

    void EqualizeSingleTile_(
        int bridgeTileX,
        int bridgeTileZ,
        float dirX,
        float dirZ,
        float bridgeHeight,
        float widthTiles,
        bool useTapering
    );

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
        bool reverseGradient,
        bool useTapering = false
    );

    void ApplyBuildableGradeToTile_(
        int tileX, int tileZ,
        float baseHeight,
        float influence,
        bool slopeInX,
        float heightStep,
        bool reverseGradient
    );
};
