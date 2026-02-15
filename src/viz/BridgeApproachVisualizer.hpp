#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atomic>
#include <d3d.h>
#include <vector>

#include "cISTETerrain.h"

struct BridgeVertex {
    float x, y, z;
    DWORD color;
};

static constexpr uint32_t kApproachColor = 0xA000FF00;      // Green, semi-transparent
static constexpr uint32_t kInvalidColor = 0xA0FF0000;       // Red, semi-transparent
static constexpr uint32_t kGridColor = 0x30FFFFFF;          // White, very transparent
static constexpr uint32_t kHeightMarkerColor = 0x80FFFF00;  // Yellow, semi-transparent
static constexpr uint32_t kGradeOkColor = 0xA000FF00;       // Green
static constexpr uint32_t kGradeWarningColor = 0xA0FFAA00;  // Orange
static constexpr uint32_t kGradeErrorColor = 0xA0FF0000;    // Red

class BridgeApproachVisualizer {
private:
    std::vector<BridgeVertex> previewVertices_;
    std::vector<BridgeVertex> activeVertices_;
    std::vector<BridgeVertex> gridVertices_;
    std::vector<BridgeVertex> heightMarkerVertices_;

public:
    // Build approach ramp preview on BOTH sides of the bridge span.
    // startX/startZ and endX/endZ define the bridge SPAN endpoints (tile coords).
    void BuildApproachPreview(
        cISTETerrain* terrain,
        int32_t startX, int32_t startZ,
        int32_t endX, int32_t endZ,
        float bridgeHeight,
        float maxGrade,
        float width,
        bool isValid
    );

    // Build height marker lines on both approaches.
    // Parameters match BuildApproachPreview (bridge span + height + grade + width).
    void BuildHeightMarkers(
        cISTETerrain* terrain,
        int32_t startX, int32_t startZ,
        int32_t endX, int32_t endZ,
        float bridgeHeight,
        float maxGrade,
        float width
    );

    // Build a tile-aligned grid overlay covering the approach areas.
    void BuildGridOverlay(
        cISTETerrain* terrain,
        int32_t startX, int32_t startZ,
        int32_t endX, int32_t endZ,
        float bridgeHeight,
        float maxGrade,
        float width
    );

    void BuildGradeVisualization(
        cISTETerrain* terrain,
        int32_t startX, int32_t startZ,
        int32_t endX, int32_t endZ,
        float bridgeHeight,
        float maxGrade,
        float width
    );

    void ClearPreview();
    void ClearAll();

    void Draw(IDirect3DDevice7* device);
};

// Global instance (like road decals)
extern BridgeApproachVisualizer gBridgeVisualizer;