#pragma once

#include "OverlayRenderer.hpp"

#include <array>
#include <cstdint>
#include <string>

class cISTETerrain;
class cIGZS3DCameraService;
struct IDirect3DDevice7;
struct S3DCameraHandle;

enum class SlopeRenderMode {
    MaxEdgeGrade,
    PlaneGrade
};

struct SlopeRenderConfig {
    SlopeRenderMode mode{SlopeRenderMode::MaxEdgeGrade};
    float opacity{0.58f};
    float minGrade{0.0f};
    std::array<float, 4> thresholds{4.0f, 10.0f, 20.0f, 35.0f};
};

class TerrainSlopeRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerSlope = 0;

    void SetEnabled(bool enabled, cISTETerrain* terrain);
    [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

    [[nodiscard]] const SlopeRenderConfig& GetConfig() const noexcept { return config_; }
    bool SetConfig(const SlopeRenderConfig& config, cISTETerrain* terrain,
                   bool rebuildIfEnabled = true, std::string* error = nullptr);
    bool ResetConfig(cISTETerrain* terrain, bool rebuildIfEnabled = true, std::string* error = nullptr);

    void Rebuild(cISTETerrain* terrain);
    void UpdateView(cISTETerrain* terrain, cIGZS3DCameraService* cameraService, IDirect3DDevice7* device);
    void ClearAll();

private:
    static constexpr float kTileSize = 16.0f;
    static constexpr float kHeightOffset = 0.20f;

    static bool ValidateConfig_(const SlopeRenderConfig& config, std::string* error = nullptr);
    static float ComputeMaxEdgeGrade_(float h00, float h10, float h01, float h11);
    static float ComputePlaneGrade_(float h00, float h10, float h01, float h11);
    static DWORD GradeColor_(float gradePercent, const SlopeRenderConfig& config);

private:
    bool enabled_{false};
    SlopeRenderConfig config_{};
};
