#pragma once
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <strsafe.h>
#include <unordered_map>
#include <vector>

struct IDirect3DDevice7;

struct OverlayVertex {
    float x, y, z;
    DWORD color;
};

class OverlayRenderer {
public:
    void EmitLine(const OverlayVertex& start, const OverlayVertex& end, float thickness, DWORD color, uint32_t layerId);
    void EmitQuad(const OverlayVertex& a, const OverlayVertex& b, const OverlayVertex& c, const OverlayVertex& d, DWORD color, uint32_t layerId);
    void Clear();
    void Draw(IDirect3DDevice7* device);
    void ClearLayer(uint32_t layerId);
    void SetLayerVisible(uint32_t layerId, bool visible);

protected:
    bool HasLayer(uint32_t layerId) const;

private:
    struct Layer {
        std::vector<OverlayVertex> vertices;
        bool visible = true;
    };
    struct SavedRenderState {
        DWORD zEnable;
        DWORD zWrite;
        DWORD lighting;
        DWORD alphaBlend;
        DWORD cullMode;
        DWORD zBias;
    };

    Layer& GetOrCreateLayer_(uint32_t);
    void SetupRenderState_(IDirect3DDevice7* device);
    void RestoreRenderState_(IDirect3DDevice7* device);

private:
    std::unordered_map<uint32_t, Layer> layers_;
    SavedRenderState savedState_{};
};
