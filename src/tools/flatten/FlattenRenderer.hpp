#pragma once

#include "viz/OverlayRenderer.hpp"
#include "FlattenOperation.hpp"

class cISTETerrain;

class FlattenRenderer : public OverlayRenderer {
public:
    static constexpr uint32_t kLayerGround = 0;
    static constexpr uint32_t kLayerFill = 1;
    static constexpr uint32_t kLayerOutline = 2;
    static constexpr uint32_t kLayerMarkers = 3;

    void Update(cISTETerrain* terrain, const FlattenPreview& preview);
    void ClearAll();

private:
    void BuildGround_(const FlattenPreview& preview);
    void BuildFill_(const FlattenPreview& preview, DWORD color);
    void BuildOutline_(const FlattenPreview& preview, DWORD color);
    void BuildMarkers_(cISTETerrain* terrain, const FlattenPreview& preview, DWORD color);
};
