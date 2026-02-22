#pragma once

#include <string>
#include <vector>

#include "OverlayRenderer.hpp"

class cISTETerrain;

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

    void SetIntervalMeters(float intervalMeters);
    [[nodiscard]] float GetIntervalMeters() const { return intervalMeters_; }

    void SetMajorEvery(int majorEvery);
    [[nodiscard]] int GetMajorEvery() const { return majorEvery_; }

    void Rebuild(cISTETerrain* terrain);
    void ClearAll();
    [[nodiscard]] const std::vector<LabelAnchor>& GetLabelAnchors() const { return labelAnchors_; }

private:
    static constexpr float kTileSize = 16.0f;
    static constexpr float kHeightOffset = 0.25f;
    static constexpr float kMinorThickness = 0.58f;
    static constexpr float kMajorThickness = 1.15f;
    static constexpr float kLabelHeightOffset = 0.30f;
    static constexpr float kLabelMinSegmentLength = 8.0f;
    static constexpr int kLabelEveryMajorSegments = 8;

    static DWORD ContourColor_(float normalizedHeight, bool major);
    static uint32_t LerpColor_(uint32_t a, uint32_t b, float t);
    static bool IsMajorLevel_(float level, float majorStep);
    static std::string FormatLevelLabel_(float levelMeters);
    static bool IntersectEdge_(float level, float hA, float hB, float xA, float zA, float xB, float zB,
                               float& outX, float& outZ);
    void TryAddLabelAnchor_(const std::string& text, float x0, float z0, float x1, float z1, float levelY,
                            int segmentCounter);

private:
    bool enabled_{false};
    float intervalMeters_{5.0f};
    int majorEvery_{5};
    std::vector<LabelAnchor> labelAnchors_{};
};
