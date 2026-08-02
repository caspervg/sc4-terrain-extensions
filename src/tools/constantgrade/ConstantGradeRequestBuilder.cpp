#include "ConstantGradeRequestBuilder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <numeric>

#include "ConstantGradeSettings.hpp"
#include "tools/bridge/BridgeApproachGeometry.hpp"

namespace {

struct TileDelta {
    int x;
    int z;
};

constexpr std::array<TileDelta, 8> kSnapDirections{{
    {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
}};

TileDelta SnapToNearestOctant(const int deltaX, const int deltaZ) noexcept {
    TileDelta best = kSnapDirections[0];
    float bestScore = std::numeric_limits<float>::lowest();

    for (const auto& candidate : kSnapDirections) {
        const float score =
            static_cast<float>(deltaX * candidate.x + deltaZ * candidate.z)
            / std::sqrt(static_cast<float>(candidate.x * candidate.x + candidate.z * candidate.z));
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    const int projection = deltaX * best.x + deltaZ * best.z;
    const int lengthSquared = best.x * best.x + best.z * best.z;
    const int steps = std::max(1, static_cast<int>(std::lround(
        static_cast<float>(projection) / static_cast<float>(lengthSquared))));

    return TileDelta{best.x * steps, best.z * steps};
}

}

std::optional<ConstantGradeRequest> BuildConstantGradeRequest(
    const ConstantGradeDragState& dragState,
    const ConstantGradeSettings& settings) {
    const int deltaX = dragState.currentX - dragState.startX;
    const int deltaZ = dragState.currentZ - dragState.startZ;
    if (deltaX == 0 && deltaZ == 0) return std::nullopt;

    if (settings.IsLineMode()) {
        const TileDelta offset = dragState.snapAngle
            ? SnapToNearestOctant(deltaX, deltaZ)
            : TileDelta{deltaX, deltaZ};

        return ConstantGradeRequest{
            .startTileX = dragState.startX,
            .startTileZ = dragState.startZ,
            .endTileX = dragState.startX + offset.x,
            .endTileZ = dragState.startZ + offset.z,
            .widthTiles = static_cast<float>(settings.lineWidthTiles.value),
            .gradePercent = settings.gradePercent.value,
            .sideSmoothing = settings.IsSideSmoothingEnabled(),
        };
    }

    const bool isHorizontal = std::abs(deltaX) >= std::abs(deltaZ);
    const int dragWidthTiles = isHorizontal ? (std::abs(deltaZ) + 1) : (std::abs(deltaX) + 1);
    const int effectiveWidthTiles = std::max(1, dragWidthTiles);
    const auto widthOffsets = BridgeApproachGeometry::GetWidthOffsetBounds(effectiveWidthTiles);

    if (isHorizontal) {
        const int minZ = std::min(dragState.startZ, dragState.currentZ);
        const int centerZ = minZ + widthOffsets.negativeOffset;
        return ConstantGradeRequest{
            .startTileX = dragState.startX,
            .startTileZ = centerZ,
            .endTileX = dragState.currentX,
            .endTileZ = centerZ,
            .widthTiles = static_cast<float>(effectiveWidthTiles),
            .gradePercent = settings.gradePercent.value,
            .sideSmoothing = settings.IsSideSmoothingEnabled(),
        };
    }

    const int minX = std::min(dragState.startX, dragState.currentX);
    const int centerX = minX + widthOffsets.negativeOffset;
    return ConstantGradeRequest{
        .startTileX = centerX,
        .startTileZ = dragState.startZ,
        .endTileX = centerX,
        .endTileZ = dragState.currentZ,
        .widthTiles = static_cast<float>(effectiveWidthTiles),
        .gradePercent = settings.gradePercent.value,
        .sideSmoothing = settings.IsSideSmoothingEnabled(),
    };
}

std::string DescribeConstantGradeDirection(const ConstantGradeRequest& request) {
    const int deltaX = request.endTileX - request.startTileX;
    const int deltaZ = request.endTileZ - request.startTileZ;
    const int divisor = std::gcd(std::abs(deltaX), std::abs(deltaZ));
    const int ratioX = divisor > 0 ? deltaX / divisor : deltaX;
    const int ratioZ = divisor > 0 ? deltaZ / divisor : deltaZ;
    const float degrees =
        std::atan2(static_cast<float>(deltaZ), static_cast<float>(deltaX))
        * (180.0f / std::numbers::pi_v<float>);

    return std::format("{}:{} ({:.1f} deg)", ratioX, ratioZ, degrees);
}
