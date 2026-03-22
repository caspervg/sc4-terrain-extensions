#include "BridgeApproachGeometry.hpp"

#include "cISTETerrain.h"

#include <algorithm>
#include <cmath>

namespace BridgeApproachGeometry {

namespace {

constexpr float kTileSizeMetres = 16.0f;
constexpr float kSafetyMargin = 1.2f;
constexpr float kMinApproachLength = 2.0f;
constexpr float kInitialApproachLength = 3.0f;
constexpr int kMaxLengthIterations = 10;
constexpr float kConvergenceThreshold = 0.5f;
constexpr float kCurrentLengthWeight = 0.3f;
constexpr float kRequiredLengthWeight = 0.7f;

} // namespace

float SampleTileAverageHeight(cISTETerrain* terrain, const int tileX, const int tileZ) {
    if (!terrain) return 0.0f;

    const float h00 = terrain->GetAltitudeAtVertex(tileX, tileZ);
    const float h10 = terrain->GetAltitudeAtVertex(tileX + 1, tileZ);
    const float h01 = terrain->GetAltitudeAtVertex(tileX, tileZ + 1);
    const float h11 = terrain->GetAltitudeAtVertex(tileX + 1, tileZ + 1);

    return (h00 + h10 + h01 + h11) * 0.25f;
}

float CalculateRequiredApproachLength(
    const float terrainHeight,
    const float bridgeHeight,
    const float maxGrade
) {
    if (maxGrade <= 0.0f) {
        return kMinApproachLength;
    }

    const float heightDiff = std::abs(bridgeHeight - terrainHeight);
    const float rawTileLength = (heightDiff * 100.0f / maxGrade) / kTileSizeMetres;
    return std::max(kMinApproachLength, std::ceil(rawTileLength * kSafetyMargin));
}

float CalculateOptimalApproachLength(
    const int bridgeX, const int bridgeZ,
    const float dirX, const float dirZ,
    const float bridgeHeight, const float maxGrade,
    const int maxTileX, const int maxTileZ,
    const TileHeightSampler sampleTileHeight,
    void* sampleContext
) {
    if (!sampleTileHeight) {
        return kMinApproachLength;
    }

    float currentLength = kInitialApproachLength;

    for (int i = 0; i < kMaxLengthIterations; ++i) {
        int sampleX = bridgeX + static_cast<int>(dirX * currentLength);
        int sampleZ = bridgeZ + static_cast<int>(dirZ * currentLength);
        sampleX = std::clamp(sampleX, 0, maxTileX);
        sampleZ = std::clamp(sampleZ, 0, maxTileZ);

        const float terrainHeight = sampleTileHeight(sampleContext, sampleX, sampleZ);
        const float requiredLength = CalculateRequiredApproachLength(
            terrainHeight, bridgeHeight, maxGrade);

        if (std::abs(requiredLength - currentLength) < kConvergenceThreshold) {
            return requiredLength;
        }

        currentLength = std::max(
            kMinApproachLength,
            kCurrentLengthWeight * currentLength +
            kRequiredLengthWeight * requiredLength
        );
    }

    return currentLength;
}

int GetEffectiveWidthTiles(const float widthTiles) {
    return std::max(1, static_cast<int>(std::round(widthTiles)));
}

WidthOffsetBounds GetWidthOffsetBounds(const int effectiveWidthTiles) {
    return WidthOffsetBounds{
        .negativeOffset = (effectiveWidthTiles - 1) / 2,
        .positiveOffset = effectiveWidthTiles / 2,
    };
}

const char* GetApproachSideModeName(const ApproachSideMode sideMode) {
    switch (sideMode) {
    case ApproachSideMode::Both:
        return "Both";
    case ApproachSideMode::Start:
        return "Start";
    case ApproachSideMode::End:
        return "End";
    case ApproachSideMode::None:
        return "None";
    }

    return "Both";
}

bool IncludesStartApproach(const ApproachSideMode sideMode) {
    return sideMode == ApproachSideMode::Both
        || sideMode == ApproachSideMode::Start;
}

bool IncludesEndApproach(const ApproachSideMode sideMode) {
    return sideMode == ApproachSideMode::Both
        || sideMode == ApproachSideMode::End;
}

ApproachParams BuildApproachParams(
    const BridgePlacement& placement,
    const float bridgeHeight,
    const float maxGrade,
    const float widthTiles,
    const int maxTileX,
    const int maxTileZ,
    const TileHeightSampler sampleTileHeight,
    void* sampleContext
) {
    ApproachParams params{};
    params.placement = placement;
    params.bridgeHeight = bridgeHeight;
    params.maxGrade = maxGrade;
    params.widthTiles = widthTiles;
    params.effectiveWidthTiles = GetEffectiveWidthTiles(widthTiles);
    params.isHorizontal = placement.isHorizontal;

    const float bridgeLength = placement.length;
    if (bridgeLength <= 0.0f) {
        return params;
    }

    const int dx = placement.bridgeEndX - placement.bridgeStartX;
    const int dz = placement.bridgeEndZ - placement.bridgeStartZ;
    const float dirX = static_cast<float>(dx) / bridgeLength;
    const float dirZ = static_cast<float>(dz) / bridgeLength;

    const float evenWidthCenterOffset =
        (params.effectiveWidthTiles % 2 == 0) ? (kTileSizeMetres * 0.5f) : 0.0f;

    params.bridgeStartWorldX = (placement.bridgeStartX + 0.5f) * kTileSizeMetres;
    params.bridgeStartWorldZ = (placement.bridgeStartZ + 0.5f) * kTileSizeMetres;
    params.bridgeEndWorldX = (placement.bridgeEndX + 0.5f) * kTileSizeMetres;
    params.bridgeEndWorldZ = (placement.bridgeEndZ + 0.5f) * kTileSizeMetres;

    if (params.isHorizontal) {
        const float centerZ = (placement.bridgeStartZ + 0.5f) * kTileSizeMetres +
            evenWidthCenterOffset;
        params.bridgeStartWorldZ = centerZ;
        params.bridgeEndWorldZ = centerZ;
        params.perpX = 0.0f;
        params.perpZ = 1.0f;
    } else {
        const float centerX = (placement.bridgeStartX + 0.5f) * kTileSizeMetres +
            evenWidthCenterOffset;
        params.bridgeStartWorldX = centerX;
        params.bridgeEndWorldX = centerX;
        params.perpX = 1.0f;
        params.perpZ = 0.0f;
    }

    params.startDirX = -dirX;
    params.startDirZ = -dirZ;
    params.endDirX = dirX;
    params.endDirZ = dirZ;

    params.startApproachLength = CalculateOptimalApproachLength(
        placement.bridgeStartX,
        placement.bridgeStartZ,
        params.startDirX,
        params.startDirZ,
        bridgeHeight,
        maxGrade,
        maxTileX,
        maxTileZ,
        sampleTileHeight,
        sampleContext
    );
    params.endApproachLength = CalculateOptimalApproachLength(
        placement.bridgeEndX,
        placement.bridgeEndZ,
        params.endDirX,
        params.endDirZ,
        bridgeHeight,
        maxGrade,
        maxTileX,
        maxTileZ,
        sampleTileHeight,
        sampleContext
    );

    return params;
}

} // namespace BridgeApproachGeometry
