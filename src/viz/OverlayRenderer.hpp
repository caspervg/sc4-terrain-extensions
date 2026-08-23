#pragma once
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <strsafe.h>
#include <unordered_map>
#include <vector>

#include <cstdint>

struct IDirect3DDevice7;
struct IDirectDrawSurface7;

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
    [[nodiscard]] bool HasVisibleGeometry() const;
    void ClearLayer(uint32_t layerId);
    void SetLayerVisible(uint32_t layerId, bool visible);

/// Appends all vertices of visible, non-empty layers to out (backend-agnostic).
    void AppendVisibleVertices(std::vector<OverlayVertex>& out) const;

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
        DWORD zFunc;
        DWORD lighting;
        DWORD fogEnable;
        DWORD rangeFogEnable;
        DWORD alphaBlend;
        DWORD alphaTest;
        DWORD alphaFunc;
        DWORD alphaRef;
        DWORD stencilEnable;
        DWORD srcBlend;
        DWORD dstBlend;
        DWORD cullMode;
        DWORD zBias;
        DWORD tss0ColorOp;
        DWORD tss0ColorArg1;
        DWORD tss0AlphaOp;
        DWORD tss0AlphaArg1;
        DWORD tss1ColorOp;
        DWORD tss1AlphaOp;
        IDirectDrawSurface7* texture0;
        IDirectDrawSurface7* texture1;
        bool okZEnable;
        bool okZWrite;
        bool okZFunc;
        bool okLighting;
        bool okFogEnable;
        bool okRangeFogEnable;
        bool okAlphaBlend;
        bool okAlphaTest;
        bool okAlphaFunc;
        bool okAlphaRef;
        bool okStencilEnable;
        bool okSrcBlend;
        bool okDstBlend;
        bool okCullMode;
        bool okZBias;
        bool okTexture0;
        bool okTexture1;
        bool okTss0ColorOp;
        bool okTss0ColorArg1;
        bool okTss0AlphaOp;
        bool okTss0AlphaArg1;
        bool okTss1ColorOp;
        bool okTss1AlphaOp;
    };

    Layer& GetOrCreateLayer_(uint32_t);
    void SetupRenderState_(IDirect3DDevice7* device);
    void RestoreRenderState_(IDirect3DDevice7* device);

private:
    std::unordered_map<uint32_t, Layer> layers_;
    SavedRenderState savedState_{};
};
