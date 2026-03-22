#pragma once

#include "ConstantGradeOperation.hpp"
#include "viz/OverlayRenderer.hpp"

class cISTETerrain;

class ConstantGradeRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerFill = 0;
    static constexpr uint32_t kLayerOutline = 1;
    static constexpr uint32_t kLayerMarkers = 2;

    void Update(cISTETerrain* terrain, const ConstantGradePreview& preview);
    void ClearAll();

private:
    void BuildFill_(const ConstantGradePreview& preview);
    void BuildOutline_(const ConstantGradePreview& preview);
    void BuildMarkers_(const ConstantGradePreview& preview);
};
