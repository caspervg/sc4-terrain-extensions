#include "TerrainSlopeRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "cISTETerrain.h"
#include "public/cIGZS3DCameraService.h"
#include "utils/Logger.h"

namespace {
DWORD PackColor(const uint8_t a, const uint8_t r, const uint8_t g, const uint8_t b) {
    return (static_cast<DWORD>(a) << 24)
        | (static_cast<DWORD>(r) << 16)
        | (static_cast<DWORD>(g) << 8)
        | static_cast<DWORD>(b);
}

uint32_t LerpColor(const uint32_t a, const uint32_t b, const float tRaw) {
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

float SmoothStep(const float edge0, const float edge1, const float x) {
    if (edge1 <= edge0) {
        return x >= edge1 ? 1.0f : 0.0f;
    }

    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

void TerrainSlopeRenderer::SetEnabled(const bool enabled, cISTETerrain* terrain) {
    enabled_ = enabled;

    if (!enabled_) {
        ClearAll();
        return;
    }

    Rebuild(terrain);
}

bool TerrainSlopeRenderer::SetConfig(
    const SlopeRenderConfig& config,
    cISTETerrain* terrain,
    const bool rebuildIfEnabled,
    std::string* error) {
    if (!ValidateConfig_(config, error)) {
        return false;
    }

    config_ = config;
    if (enabled_ && rebuildIfEnabled) {
        Rebuild(terrain);
    }
    return true;
}

bool TerrainSlopeRenderer::ResetConfig(cISTETerrain* terrain, const bool rebuildIfEnabled, std::string* error) {
    return SetConfig(SlopeRenderConfig{}, terrain, rebuildIfEnabled, error);
}

void TerrainSlopeRenderer::Rebuild(cISTETerrain* terrain) {
    ClearLayer(kLayerSlope);
    if (!enabled_ || !terrain) {
        return;
    }

    const uint32_t cellsX = terrain->CellCountX();
    const uint32_t cellsZ = terrain->CellCountZ();
    if (cellsX == 0 || cellsZ == 0) {
        return;
    }

    for (uint32_t tz = 0; tz < cellsZ; ++tz) {
        for (uint32_t tx = 0; tx < cellsX; ++tx) {
            const int x0 = static_cast<int>(tx);
            const int z0 = static_cast<int>(tz);
            const int x1 = x0 + 1;
            const int z1 = z0 + 1;

            const float h00 = terrain->GetAltitudeAtVertex(x0, z0);
            const float h10 = terrain->GetAltitudeAtVertex(x1, z0);
            const float h01 = terrain->GetAltitudeAtVertex(x0, z1);
            const float h11 = terrain->GetAltitudeAtVertex(x1, z1);

            const float grade = config_.mode == SlopeRenderMode::PlaneGrade
                ? ComputePlaneGrade_(h00, h10, h01, h11)
                : ComputeMaxEdgeGrade_(h00, h10, h01, h11);

            if (grade < config_.minGrade) {
                continue;
            }

            const DWORD color = GradeColor_(grade, config_);
            const float wx0 = static_cast<float>(x0) * kTileSize;
            const float wz0 = static_cast<float>(z0) * kTileSize;
            const float wx1 = static_cast<float>(x1) * kTileSize;
            const float wz1 = static_cast<float>(z1) * kTileSize;

            EmitQuad(
                {wx0, h00 + kHeightOffset, wz0, color},
                {wx1, h10 + kHeightOffset, wz0, color},
                {wx1, h11 + kHeightOffset, wz1, color},
                {wx0, h01 + kHeightOffset, wz1, color},
                color,
                kLayerSlope);
        }
    }

    LOG_DEBUG("TerrainSlopeRenderer: rebuilt slope heatmap mode={} opacity={} min-grade={}",
        config_.mode == SlopeRenderMode::PlaneGrade ? "plane-grade" : "max-edge-grade",
        config_.opacity,
        config_.minGrade);
}

void TerrainSlopeRenderer::UpdateView(
    cISTETerrain* terrain,
    cIGZS3DCameraService* cameraService,
    IDirect3DDevice7* device) {
    (void)cameraService;
    (void)device;
    if (enabled_ && terrain && !HasLayer(kLayerSlope)) {
        Rebuild(terrain);
    }
}

void TerrainSlopeRenderer::ClearAll() {
    ClearLayer(kLayerSlope);
}

bool TerrainSlopeRenderer::ValidateConfig_(const SlopeRenderConfig& config, std::string* error) {
    if (config.opacity < 0.0f || config.opacity > 1.0f) {
        if (error) *error = "--opacity must be in the range 0..1.";
        return false;
    }
    if (config.minGrade < 0.0f) {
        if (error) *error = "--min-grade must be >= 0.";
        return false;
    }
    if (!(config.thresholds[0] > 0.0f
        && config.thresholds[0] < config.thresholds[1]
        && config.thresholds[1] < config.thresholds[2]
        && config.thresholds[2] < config.thresholds[3])) {
        if (error) *error = "--thresholds must be four ascending positive values.";
        return false;
    }
    return true;
}

float TerrainSlopeRenderer::ComputeMaxEdgeGrade_(const float h00, const float h10, const float h01, const float h11) {
    constexpr float kDiagTileSize = kTileSize * 1.41421356237f;

    const std::array edgeGrades{
        std::abs(h10 - h00) / kTileSize * 100.0f,
        std::abs(h11 - h10) / kTileSize * 100.0f,
        std::abs(h11 - h01) / kTileSize * 100.0f,
        std::abs(h01 - h00) / kTileSize * 100.0f,
        std::abs(h11 - h00) / kDiagTileSize * 100.0f,
        std::abs(h10 - h01) / kDiagTileSize * 100.0f
    };

    return *std::max_element(edgeGrades.begin(), edgeGrades.end());
}

float TerrainSlopeRenderer::ComputePlaneGrade_(const float h00, const float h10, const float h01, const float h11) {
    const float dzdx = ((h10 + h11) - (h00 + h01)) / (2.0f * kTileSize);
    const float dzdz = ((h01 + h11) - (h00 + h10)) / (2.0f * kTileSize);
    return std::sqrt(dzdx * dzdx + dzdz * dzdz) * 100.0f;
}

DWORD TerrainSlopeRenderer::GradeColor_(const float gradePercent, const SlopeRenderConfig& config) {
    const uint8_t alpha = static_cast<uint8_t>(std::round(std::clamp(config.opacity, 0.0f, 1.0f) * 255.0f));
    const auto& t = config.thresholds;
    constexpr uint32_t c0 = 0x4CA46D;
    constexpr uint32_t c1 = 0x9CCB5B;
    constexpr uint32_t c2 = 0xE9C85B;
    constexpr uint32_t c3 = 0xE69A43;
    constexpr uint32_t c4 = 0xD85A3F;
    constexpr uint32_t c5 = 0x9A3D68;

    const float lowStart = std::max(0.0f, config.minGrade);
    const float highEnd = std::max(t[3] + 30.0f, t[3] * 2.5f);

    uint32_t rgb = c0;
    if (gradePercent <= t[0]) {
        const float s = SmoothStep(lowStart, t[0], gradePercent);
        rgb = LerpColor(c0, c1, s);
    } else if (gradePercent <= t[1]) {
        const float s = SmoothStep(t[0], t[1], gradePercent);
        rgb = LerpColor(c1, c2, s);
    } else if (gradePercent <= t[2]) {
        const float s = SmoothStep(t[1], t[2], gradePercent);
        rgb = LerpColor(c2, c3, s);
    } else if (gradePercent <= t[3]) {
        const float s = SmoothStep(t[2], t[3], gradePercent);
        rgb = LerpColor(c3, c4, s);
    } else {
        const float s = SmoothStep(t[3], highEnd, gradePercent);
        rgb = LerpColor(c4, c5, s);
    }

    return (static_cast<DWORD>(alpha) << 24)
        | (static_cast<DWORD>((rgb >> 16) & 0xFFu) << 16)
        | (static_cast<DWORD>((rgb >> 8) & 0xFFu) << 8)
        | static_cast<DWORD>(rgb & 0xFFu);
}
