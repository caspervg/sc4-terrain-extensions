#pragma once

#include "ConstantGradeOperation.hpp"
#include "viz/OverlayRenderer.hpp"

class cISTETerrain;

class ConstantGradeRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerGround = 0;
    static constexpr uint32_t kLayerFill = 1;
    static constexpr uint32_t kLayerOutline = 2;
    static constexpr uint32_t kLayerMarkers = 3;
    static constexpr uint32_t kLayerHover = 4;

    void Update(cISTETerrain* terrain, const ConstantGradePreview& preview);
    void ShowHoverTile(cISTETerrain* terrain, int tileX, int tileZ);
    void ClearHoverTile();
    void ClearAll();

private:
    void BuildGround_(const ConstantGradePreview& preview);
    void BuildFill_(const ConstantGradePreview& preview, DWORD color);
    void BuildOutline_(const ConstantGradePreview& preview, DWORD color);
    void BuildMarkers_(const ConstantGradePreview& preview);
};
