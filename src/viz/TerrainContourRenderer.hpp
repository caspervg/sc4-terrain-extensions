#pragma once

#include <string>
#include <vector>

#include "OverlayRenderer.hpp"

class cISTETerrain;

enum class ContourMajorLineMode {
    EveryNth,
    HeightStep
};

struct ContourRenderConfig {
    float intervalMeters{5.0f};
    float minorThickness{0.90f};
    float majorThickness{1.70f};
    float minorOpacity{0.72f};
    float majorOpacity{0.96f};
    ContourMajorLineMode majorLineMode{ContourMajorLineMode::EveryNth};
    int majorEvery{5};
    float majorHeightStepMeters{25.0f};
};

class TerrainContourRenderer : public OverlayRenderer {
public:
    struct LabelAnchor {
        float worldX;
        float worldY;
        float worldZ;
        std::string text;
    };

    static constexpr uint32_t kLayerContours = 0;

    void SetEnabled(bool enabled, cISTETerrain* terrain);
    [[nodiscard]] bool IsEnabled() const { return enabled_; }

    [[nodiscard]] const ContourRenderConfig& GetConfig() const { return config_; }
    bool SetConfig(const ContourRenderConfig& config, cISTETerrain* terrain,
                   bool rebuildIfEnabled = true, std::string* error = nullptr);
    bool ResetConfig(cISTETerrain* terrain, bool rebuildIfEnabled = true, std::string* error = nullptr);

    void Rebuild(cISTETerrain* terrain);
    void ClearAll();
    [[nodiscard]] const std::vector<LabelAnchor>& GetLabelAnchors() const { return labelAnchors_; }

private:
    static constexpr float kTileSize = 16.0f;
    static constexpr float kHeightOffset = 0.25f;
    static constexpr float kLabelHeightOffset = 0.30f;
    static constexpr float kLabelMinSegmentLength = 8.0f;
    static constexpr int kLabelEveryMajorSegments = 2;

    static bool ValidateConfig_(const ContourRenderConfig& config, std::string* error = nullptr);
    static DWORD ContourColor_(float normalizedHeight, bool major, const ContourRenderConfig& config);
    static uint32_t LerpColor_(uint32_t a, uint32_t b, float t);
    static bool IsMajorLevel_(float level, int contourIndex, const ContourRenderConfig& config);
    static std::string FormatLevelLabel_(float levelMeters);
    static bool IntersectEdge_(float level, float hA, float hB, float xA, float zA, float xB, float zB,
                               float& outX, float& outZ);
    void TryAddLabelAnchor_(const std::string& text, float x0, float z0, float x1, float z1, float levelY,
                            int segmentCounter);

private:
    bool enabled_{false};
    ContourRenderConfig config_{};
    std::vector<LabelAnchor> labelAnchors_{};
};
