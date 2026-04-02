#include "TerrainContourRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "cISTETerrain.h"
#include "utils/Logger.h"

namespace {
struct ContourPoint {
    float x;
    float z;
};

constexpr float kEdgeEpsilon = 1e-5f;
constexpr float kPointDedupEpsilon = 1e-3f;
constexpr size_t kMaxLabelAnchors = 220;
constexpr float kMinLabelAnchorSpacing = 96.0f; // world units

void TrimTrailingZeros(std::string& numeric) {
    while (!numeric.empty() && numeric.back() == '0') {
        numeric.pop_back();
    }
    if (!numeric.empty() && numeric.back() == '.') {
        numeric.pop_back();
    }
}
}

void TerrainContourRenderer::SetEnabled(const bool enabled, cISTETerrain* terrain) {
    enabled_ = enabled;

    if (!enabled_) {
        ClearAll();
        return;
    }

    Rebuild(terrain);
}

bool TerrainContourRenderer::SetConfig(const ContourRenderConfig& config, cISTETerrain* terrain,
                                       const bool rebuildIfEnabled, std::string* error) {
    if (!ValidateConfig_(config, error)) {
        return false;
    }

    config_ = config;

    if (enabled_ && rebuildIfEnabled) {
        Rebuild(terrain);
    }

    return true;
}

bool TerrainContourRenderer::ResetConfig(cISTETerrain* terrain, const bool rebuildIfEnabled, std::string* error) {
    return SetConfig(ContourRenderConfig{}, terrain, rebuildIfEnabled, error);
}

void TerrainContourRenderer::Rebuild(cISTETerrain* terrain) {
    ClearLayer(kLayerContours);
    labelAnchors_.clear();
    if (!enabled_ || !terrain) return;

    const uint32_t verticesX = terrain->CellCountX() + 1;
    const uint32_t verticesZ = terrain->CellCountZ() + 1;
    if (verticesX < 2 || verticesZ < 2) return;

    const size_t totalVertices = static_cast<size_t>(verticesX) * verticesZ;
    std::vector heights(totalVertices, 0.0f);

    float minHeight = std::numeric_limits<float>::max();
    float maxHeight = std::numeric_limits<float>::lowest();

    for (uint32_t z = 0; z < verticesZ; ++z) {
        for (uint32_t x = 0; x < verticesX; ++x) {
            const float h = terrain->GetAltitudeAtVertex(static_cast<int>(x), static_cast<int>(z));
            heights[z * verticesX + x] = h;
            minHeight = std::min(minHeight, h);
            maxHeight = std::max(maxHeight, h);
        }
    }

    if (!(maxHeight > minHeight + config_.intervalMeters * 0.25f)) return;

    const float startLevel = std::floor(minHeight / config_.intervalMeters) * config_.intervalMeters;
    const float endLevel = std::ceil(maxHeight / config_.intervalMeters) * config_.intervalMeters;
    const uint32_t cellsX = verticesX - 1;
    const uint32_t cellsZ = verticesZ - 1;

    int contourIndex = 0;
    for (float level = startLevel; level <= endLevel + kEdgeEpsilon; level += config_.intervalMeters, ++contourIndex) {
        const bool major = IsMajorLevel_(level, contourIndex, config_);
        const float thickness = major ? config_.majorThickness : config_.minorThickness;
        const std::string majorLabelText = major ? FormatLevelLabel_(level) : std::string{};
        int majorSegmentCounter = 0;

        const float normalized = (maxHeight > minHeight)
            ? std::clamp((level - minHeight) / (maxHeight - minHeight), 0.0f, 1.0f)
            : 0.5f;
        const DWORD color = ContourColor_(normalized, major, config_);

        for (uint32_t tz = 0; tz < cellsZ; ++tz) {
            for (uint32_t tx = 0; tx < cellsX; ++tx) {
                const int x0 = static_cast<int>(tx);
                const int z0 = static_cast<int>(tz);
                const int x1 = x0 + 1;
                const int z1 = z0 + 1;

                const float h00 = heights[static_cast<size_t>(z0) * verticesX + x0];
                const float h10 = heights[static_cast<size_t>(z0) * verticesX + x1];
                const float h01 = heights[static_cast<size_t>(z1) * verticesX + x0];
                const float h11 = heights[static_cast<size_t>(z1) * verticesX + x1];

                const float localMin = std::min(std::min(h00, h10), std::min(h01, h11));
                const float localMax = std::max(std::max(h00, h10), std::max(h01, h11));
                if (level < localMin || level > localMax) continue;

                std::array<ContourPoint, 4> rawPoints{};
                int rawCount = 0;

                auto addIntersection = [&](float ha, float hb, float ax, float az, float bx, float bz) {
                    float ix = 0.0f;
                    float iz = 0.0f;
                    if (!IntersectEdge_(level, ha, hb, ax, az, bx, bz, ix, iz)) return;
                    if (rawCount < 4) {
                        rawPoints[rawCount++] = {ix, iz};
                    }
                };

                addIntersection(h00, h10, static_cast<float>(x0), static_cast<float>(z0),
                    static_cast<float>(x1), static_cast<float>(z0)); // Top
                addIntersection(h10, h11, static_cast<float>(x1), static_cast<float>(z0),
                    static_cast<float>(x1), static_cast<float>(z1)); // Right
                addIntersection(h11, h01, static_cast<float>(x1), static_cast<float>(z1),
                    static_cast<float>(x0), static_cast<float>(z1)); // Bottom
                addIntersection(h01, h00, static_cast<float>(x0), static_cast<float>(z1),
                    static_cast<float>(x0), static_cast<float>(z0)); // Left

                if (rawCount < 2) continue;

                std::vector<ContourPoint> points;
                points.reserve(static_cast<size_t>(rawCount));
                for (int i = 0; i < rawCount; ++i) {
                    const ContourPoint p = rawPoints[i];
                    const bool duplicate = std::any_of(points.begin(), points.end(), [&](const ContourPoint& existing) {
                        return std::abs(existing.x - p.x) < kPointDedupEpsilon
                            && std::abs(existing.z - p.z) < kPointDedupEpsilon;
                    });
                    if (!duplicate) {
                        points.push_back(p);
                    }
                }

                if (points.size() == 2) {
                    const float y = level + kHeightOffset;
                    const float wx0 = points[0].x * kTileSize;
                    const float wz0 = points[0].z * kTileSize;
                    const float wx1 = points[1].x * kTileSize;
                    const float wz1 = points[1].z * kTileSize;
                    EmitLine({wx0, y, wz0, color}, {wx1, y, wz1, color}, thickness, color, kLayerContours);

                    if (major) {
                        TryAddLabelAnchor_(majorLabelText, wx0, wz0, wx1, wz1, y + kLabelHeightOffset, majorSegmentCounter);
                        ++majorSegmentCounter;
                    }
                } else if (points.size() == 4) {
                    // Saddle cell case: pair by nearest-neighbor to avoid crossing segments.
                    const auto dist2 = [](const ContourPoint& a, const ContourPoint& b) {
                        const float dx = a.x - b.x;
                        const float dz = a.z - b.z;
                        return dx * dx + dz * dz;
                    };

                    float bestDistance = std::numeric_limits<float>::max();
                    int i0 = 0;
                    int i1 = 1;
                    for (int a = 0; a < 4; ++a) {
                        for (int b = a + 1; b < 4; ++b) {
                            const float d = dist2(points[a], points[b]);
                            if (d < bestDistance) {
                                bestDistance = d;
                                i0 = a;
                                i1 = b;
                            }
                        }
                    }

                    std::array<int, 2> rest{};
                    int idx = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (i != i0 && i != i1) rest[idx++] = i;
                    }

                    const float y = level + kHeightOffset;
                    const float wx0 = points[i0].x * kTileSize;
                    const float wz0 = points[i0].z * kTileSize;
                    const float wx1 = points[i1].x * kTileSize;
                    const float wz1 = points[i1].z * kTileSize;
                    EmitLine({wx0, y, wz0, color}, {wx1, y, wz1, color}, thickness, color, kLayerContours);

                    if (major) {
                        TryAddLabelAnchor_(majorLabelText, wx0, wz0, wx1, wz1, y + kLabelHeightOffset, majorSegmentCounter);
                        ++majorSegmentCounter;
                    }

                    const float wx2 = points[rest[0]].x * kTileSize;
                    const float wz2 = points[rest[0]].z * kTileSize;
                    const float wx3 = points[rest[1]].x * kTileSize;
                    const float wz3 = points[rest[1]].z * kTileSize;
                    EmitLine({wx2, y, wz2, color}, {wx3, y, wz3, color}, thickness, color, kLayerContours);

                    if (major) {
                        TryAddLabelAnchor_(majorLabelText, wx2, wz2, wx3, wz3, y + kLabelHeightOffset, majorSegmentCounter);
                        ++majorSegmentCounter;
                    }
                }
            }
        }
    }

    LOG_DEBUG("TerrainContourRenderer: rebuilt contour map (interval={}m minor-thickness={} major-thickness={} major-lines-by={} labels={})",
              config_.intervalMeters,
              config_.minorThickness,
              config_.majorThickness,
              config_.majorLineMode == ContourMajorLineMode::EveryNth ? "every-nth" : "height-step",
              labelAnchors_.size());
}

void TerrainContourRenderer::ClearAll() {
    ClearLayer(kLayerContours);
    labelAnchors_.clear();
}

bool TerrainContourRenderer::ValidateConfig_(const ContourRenderConfig& config, std::string* error) {
    if (config.intervalMeters <= 0.0f) {
        if (error) *error = "--interval must be > 0.";
        return false;
    }
    if (config.minorThickness <= 0.0f) {
        if (error) *error = "--minor-thickness must be > 0.";
        return false;
    }
    if (config.majorThickness <= 0.0f) {
        if (error) *error = "--major-thickness must be > 0.";
        return false;
    }
    if (config.minorOpacity < 0.0f || config.minorOpacity > 1.0f) {
        if (error) *error = "--minor-opacity must be in the range 0..1.";
        return false;
    }
    if (config.majorOpacity < 0.0f || config.majorOpacity > 1.0f) {
        if (error) *error = "--major-opacity must be in the range 0..1.";
        return false;
    }
    if (config.majorEvery < 1) {
        if (error) *error = "--major-every must be >= 1.";
        return false;
    }
    if (config.majorHeightStepMeters <= 0.0f) {
        if (error) *error = "--major-height-step must be > 0.";
        return false;
    }
    return true;
}

DWORD TerrainContourRenderer::ContourColor_(const float normalizedHeight, const bool major, const ContourRenderConfig& config) {
    constexpr uint32_t lowColor = 0x5C67C8;  // blue
    constexpr uint32_t midColor = 0x66A45F;  // green
    constexpr uint32_t highColor = 0xC39B6A; // tan
    constexpr uint32_t peakColor = 0xE8E8E8; // near-white

    const uint32_t rgb = [&]() {
        if (normalizedHeight < 0.33f) {
            return LerpColor_(lowColor, midColor, normalizedHeight / 0.33f);
        }
        if (normalizedHeight < 0.66f) {
            return LerpColor_(midColor, highColor, (normalizedHeight - 0.33f) / 0.33f);
        }
        return LerpColor_(highColor, peakColor, (normalizedHeight - 0.66f) / 0.34f);
    }();

    const float opacity = std::clamp(major ? config.majorOpacity : config.minorOpacity, 0.0f, 1.0f);
    const uint8_t alpha = static_cast<uint8_t>(std::round(opacity * 255.0f));
    return (static_cast<DWORD>(alpha) << 24)
        | (static_cast<DWORD>((rgb >> 16) & 0xFFu) << 16)
        | (static_cast<DWORD>((rgb >> 8) & 0xFFu) << 8)
        | static_cast<DWORD>(rgb & 0xFFu);
}

uint32_t TerrainContourRenderer::LerpColor_(const uint32_t a, const uint32_t b, const float tRaw) {
    const float t = std::clamp(tRaw, 0.0f, 1.0f);
    const auto ar = static_cast<float>((a >> 16) & 0xFFu);
    const auto ag = static_cast<float>((a >> 8) & 0xFFu);
    const auto ab = static_cast<float>(a & 0xFFu);

    const auto br = static_cast<float>((b >> 16) & 0xFFu);
    const auto bg = static_cast<float>((b >> 8) & 0xFFu);
    const auto bb = static_cast<float>(b & 0xFFu);

    const auto rr = static_cast<uint32_t>(std::round(ar + (br - ar) * t));
    const auto rg = static_cast<uint32_t>(std::round(ag + (bg - ag) * t));
    const auto rb = static_cast<uint32_t>(std::round(ab + (bb - ab) * t));

    return (rr << 16) | (rg << 8) | rb;
}

bool TerrainContourRenderer::IsMajorLevel_(const float level, const int contourIndex, const ContourRenderConfig& config) {
    if (config.majorLineMode == ContourMajorLineMode::EveryNth) {
        return contourIndex % std::max(1, config.majorEvery) == 0;
    }

    const float majorStep = config.majorHeightStepMeters;
    const float nearestMajor = std::round(level / majorStep) * majorStep;
    const float epsilon = std::max(0.0005f, majorStep * 0.001f);
    return std::abs(level - nearestMajor) <= epsilon;
}

std::string TerrainContourRenderer::FormatLevelLabel_(float levelMeters) {
    if (std::abs(levelMeters) < 1e-4f) {
        levelMeters = 0.0f;
    }

    const float rounded2 = std::round(levelMeters * 100.0f) / 100.0f;
    const float nearestInt = std::round(rounded2);
    const float nearestTenth = std::round(rounded2 * 10.0f) / 10.0f;

    int precision = 2;
    if (std::abs(rounded2 - nearestInt) < 1e-3f) {
        precision = 0;
    } else if (std::abs(rounded2 - nearestTenth) < 1e-3f) {
        precision = 1;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << rounded2;
    std::string numeric = oss.str();
    if (precision > 0) {
        TrimTrailingZeros(numeric);
    }

    return numeric + "m";
}

void TerrainContourRenderer::TryAddLabelAnchor_(const std::string& text,
                                                const float x0, const float z0,
                                                const float x1, const float z1,
                                                const float levelY,
                                                const int segmentCounter) {
    if (text.empty()) return;
    if ((segmentCounter % kLabelEveryMajorSegments) != 0) return;
    if (labelAnchors_.size() >= kMaxLabelAnchors) return;

    const float dx = x1 - x0;
    const float dz = z1 - z0;
    const float len = std::sqrt(dx * dx + dz * dz);
    if (len < kLabelMinSegmentLength) return;

    const float midX = (x0 + x1) * 0.5f;
    const float midZ = (z0 + z1) * 0.5f;

    const bool tooClose = std::ranges::any_of(labelAnchors_, [&](const LabelAnchor& a) {
        const float sx = a.worldX - midX;
        const float sz = a.worldZ - midZ;
        return (sx * sx + sz * sz) < (kMinLabelAnchorSpacing * kMinLabelAnchorSpacing);
    });
    if (tooClose) return;

    labelAnchors_.push_back(LabelAnchor{
        midX,
        levelY,
        midZ,
        text
    });
}

bool TerrainContourRenderer::IntersectEdge_(const float level, const float hA, const float hB,
                                            const float xA, const float zA, const float xB, const float zB,
                                            float& outX, float& outZ) {
    const float dA = hA - level;
    const float dB = hB - level;

    // Skip fully flat edges exactly on the contour to avoid heavy duplicate geometry.
    if (std::abs(dA) < kEdgeEpsilon && std::abs(dB) < kEdgeEpsilon) {
        return false;
    }

    if ((dA > 0.0f && dB > 0.0f) || (dA < 0.0f && dB < 0.0f)) {
        return false;
    }

    const float denom = hB - hA;
    if (std::abs(denom) < kEdgeEpsilon) {
        return false;
    }

    const float t = std::clamp((level - hA) / denom, 0.0f, 1.0f);
    outX = xA + (xB - xA) * t;
    outZ = zA + (zB - zA) * t;
    return true;
}
