#pragma once

#include <cstdint>
#include <optional>
#include <cmath>

// The computed tile coordinates of a bridge span, derived from drag start/end.
// Axis-aligned only — the longer axis wins, shorter axis is ignored.
struct BridgePlacement {
    int32_t bridgeStartX;
    int32_t bridgeStartZ;
    int32_t bridgeEndX;
    int32_t bridgeEndZ;

    float length; // in tiles, along the dominant axis
    bool isHorizontal; // true = spans along X, false = spans along Z

    static constexpr auto kMinLength = 3.0f;

    [[nodiscard]] bool IsValid() const noexcept {
        return length >= kMinLength;
    }
};

// Pure function — no terrain access, no state.
// Returns nullopt if start == end.
inline std::optional<BridgePlacement> ComputeBridgePlacement(
    int32_t startX, int32_t startZ,
    int32_t endX, int32_t endZ) {
    const int32_t dx = endX - startX;
    const int32_t dz = endZ - startZ;

    if (dx == 0 && dz == 0) return std::nullopt;

    const bool isHorizontal = std::abs(dx) >= std::abs(dz);

    // Snap to dominant axis — bridge approaches only support axis-aligned spans
    const int32_t snappedEndX = isHorizontal ? endX : startX;
    const int32_t snappedEndZ = isHorizontal ? startZ : endZ;

    const int32_t snappedDx = snappedEndX - startX;
    const int32_t snappedDz = snappedEndZ - startZ;

    const float length = std::sqrt(
        static_cast<float>(snappedDx * snappedDx + snappedDz * snappedDz));

    return BridgePlacement{
        .bridgeStartX = startX,
        .bridgeStartZ = startZ,
        .bridgeEndX = snappedEndX,
        .bridgeEndZ = snappedEndZ,
        .length = length,
        .isHorizontal = isHorizontal,
    };
}
