#pragma once

#include <cstdint>

#include "BridgePlacement.hpp"

class cISTETerrain;

namespace BridgeApproachGeometry {

struct WidthOffsetBounds {
    int negativeOffset;
    int positiveOffset;
};

struct ApproachParams {
    BridgePlacement placement{};
    float bridgeHeight{0.0f};
    float maxGrade{0.0f};
    float widthTiles{0.0f};
    int effectiveWidthTiles{1};

    float bridgeStartWorldX{0.0f};
    float bridgeStartWorldZ{0.0f};
    float bridgeEndWorldX{0.0f};
    float bridgeEndWorldZ{0.0f};
    float startDirX{0.0f};
    float startDirZ{0.0f};
    float endDirX{0.0f};
    float endDirZ{0.0f};
    float perpX{0.0f};
    float perpZ{0.0f};
    float startApproachLength{0.0f};
    float endApproachLength{0.0f};
    bool isHorizontal{false};
};

using TileHeightSampler = float (*)(void* context, int tileX, int tileZ);

float SampleTileAverageHeight(cISTETerrain* terrain, int tileX, int tileZ);

float CalculateRequiredApproachLength(float terrainHeight, float bridgeHeight, float maxGrade);

float CalculateOptimalApproachLength(
    int bridgeX, int bridgeZ,
    float dirX, float dirZ,
    float bridgeHeight, float maxGrade,
    int maxTileX, int maxTileZ,
    TileHeightSampler sampleTileHeight,
    void* sampleContext
);

int GetEffectiveWidthTiles(float widthTiles);

WidthOffsetBounds GetWidthOffsetBounds(int effectiveWidthTiles);

ApproachParams BuildApproachParams(
    const BridgePlacement& placement,
    float bridgeHeight,
    float maxGrade,
    float widthTiles,
    int maxTileX,
    int maxTileZ,
    TileHeightSampler sampleTileHeight,
    void* sampleContext
);

} // namespace BridgeApproachGeometry
