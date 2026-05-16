#pragma once

#include <d3d.h>
#include <ddraw.h>
#include <cstdint>

struct D3D7StateGuard {
    explicit D3D7StateGuard(IDirect3DDevice7* dev) : device(dev) {
        if (!device) return;
        CaptureTransform(D3DTRANSFORMSTATE_WORLD, world, Capture_World);
        CaptureTransform(D3DTRANSFORMSTATE_VIEW, view, Capture_View);
        CaptureTransform(D3DTRANSFORMSTATE_PROJECTION, projection, Capture_Projection);

        CaptureRenderState(D3DRENDERSTATE_ZENABLE, zEnable, Capture_ZEnable);
        CaptureRenderState(D3DRENDERSTATE_ZWRITEENABLE, zWrite, Capture_ZWrite);
        CaptureRenderState(D3DRENDERSTATE_ZFUNC, zFunc, Capture_ZFunc);
        CaptureRenderState(D3DRENDERSTATE_LIGHTING, lighting, Capture_Lighting);
        CaptureRenderState(D3DRENDERSTATE_SHADEMODE, shadeMode, Capture_ShadeMode);
        CaptureRenderState(D3DRENDERSTATE_FOGENABLE, fogEnable, Capture_FogEnable);
        CaptureRenderState(D3DRENDERSTATE_RANGEFOGENABLE, rangeFogEnable, Capture_RangeFogEnable);
        CaptureRenderState(D3DRENDERSTATE_CLIPPING, clipping, Capture_Clipping);
        CaptureRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, alphaBlend, Capture_AlphaBlend);
        CaptureRenderState(D3DRENDERSTATE_ALPHATESTENABLE, alphaTest, Capture_AlphaTest);
        CaptureRenderState(D3DRENDERSTATE_ALPHAFUNC, alphaFunc, Capture_AlphaFunc);
        CaptureRenderState(D3DRENDERSTATE_ALPHAREF, alphaRef, Capture_AlphaRef);
        CaptureRenderState(D3DRENDERSTATE_SRCBLEND, srcBlend, Capture_SrcBlend);
        CaptureRenderState(D3DRENDERSTATE_DESTBLEND, destBlend, Capture_DestBlend);
        CaptureRenderState(D3DRENDERSTATE_CULLMODE, cullMode, Capture_CullMode);
        CaptureRenderState(D3DRENDERSTATE_ZBIAS, zBias, Capture_ZBias);
        CaptureRenderState(D3DRENDERSTATE_STENCILENABLE, stencilEnable, Capture_StencilEnable);

        CaptureTexture(0, texture0, Capture_Texture0);
        CaptureTexture(1, texture1, Capture_Texture1);

        CaptureTextureStageState(0, D3DTSS_COLOROP, tss0ColorOp, Capture_TSS0ColorOp);
        CaptureTextureStageState(0, D3DTSS_COLORARG1, tss0ColorArg1, Capture_TSS0ColorArg1);
        CaptureTextureStageState(0, D3DTSS_COLORARG2, tss0ColorArg2, Capture_TSS0ColorArg2);
        CaptureTextureStageState(0, D3DTSS_ALPHAOP, tss0AlphaOp, Capture_TSS0AlphaOp);
        CaptureTextureStageState(0, D3DTSS_ALPHAARG1, tss0AlphaArg1, Capture_TSS0AlphaArg1);
        CaptureTextureStageState(0, D3DTSS_ALPHAARG2, tss0AlphaArg2, Capture_TSS0AlphaArg2);
        CaptureTextureStageState(0, D3DTSS_MINFILTER, tss0MinFilter, Capture_TSS0MinFilter);
        CaptureTextureStageState(0, D3DTSS_MAGFILTER, tss0MagFilter, Capture_TSS0MagFilter);
        CaptureTextureStageState(0, D3DTSS_MIPFILTER, tss0MipFilter, Capture_TSS0MipFilter);
        CaptureTextureStageState(0, D3DTSS_ADDRESSU, tss0AddressU, Capture_TSS0AddressU);
        CaptureTextureStageState(0, D3DTSS_ADDRESSV, tss0AddressV, Capture_TSS0AddressV);
        CaptureTextureStageState(0, D3DTSS_TEXCOORDINDEX, tss0TexCoordIndex, Capture_TSS0TexCoordIndex);
        CaptureTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, tss0TextureTransformFlags, Capture_TSS0TextureTransformFlags);
        CaptureTextureStageState(1, D3DTSS_COLOROP, tss1ColorOp, Capture_TSS1ColorOp);
        CaptureTextureStageState(1, D3DTSS_ALPHAOP, tss1AlphaOp, Capture_TSS1AlphaOp);

        if (SUCCEEDED(device->GetViewport(&viewport))) capturedFlags |= Capture_Viewport;
    }

    ~D3D7StateGuard() {
        if (!device) return;
        RestoreTransform(D3DTRANSFORMSTATE_WORLD, world, Capture_World);
        RestoreTransform(D3DTRANSFORMSTATE_VIEW, view, Capture_View);
        RestoreTransform(D3DTRANSFORMSTATE_PROJECTION, projection, Capture_Projection);

        RestoreRenderState(D3DRENDERSTATE_ZENABLE, zEnable, Capture_ZEnable);
        RestoreRenderState(D3DRENDERSTATE_ZWRITEENABLE, zWrite, Capture_ZWrite);
        RestoreRenderState(D3DRENDERSTATE_ZFUNC, zFunc, Capture_ZFunc);
        RestoreRenderState(D3DRENDERSTATE_LIGHTING, lighting, Capture_Lighting);
        RestoreRenderState(D3DRENDERSTATE_SHADEMODE, shadeMode, Capture_ShadeMode);
        RestoreRenderState(D3DRENDERSTATE_FOGENABLE, fogEnable, Capture_FogEnable);
        RestoreRenderState(D3DRENDERSTATE_RANGEFOGENABLE, rangeFogEnable, Capture_RangeFogEnable);
        RestoreRenderState(D3DRENDERSTATE_CLIPPING, clipping, Capture_Clipping);
        RestoreRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, alphaBlend, Capture_AlphaBlend);
        RestoreRenderState(D3DRENDERSTATE_ALPHATESTENABLE, alphaTest, Capture_AlphaTest);
        RestoreRenderState(D3DRENDERSTATE_ALPHAFUNC, alphaFunc, Capture_AlphaFunc);
        RestoreRenderState(D3DRENDERSTATE_ALPHAREF, alphaRef, Capture_AlphaRef);
        RestoreRenderState(D3DRENDERSTATE_SRCBLEND, srcBlend, Capture_SrcBlend);
        RestoreRenderState(D3DRENDERSTATE_DESTBLEND, destBlend, Capture_DestBlend);
        RestoreRenderState(D3DRENDERSTATE_CULLMODE, cullMode, Capture_CullMode);
        RestoreRenderState(D3DRENDERSTATE_ZBIAS, zBias, Capture_ZBias);
        RestoreRenderState(D3DRENDERSTATE_STENCILENABLE, stencilEnable, Capture_StencilEnable);

        RestoreTextureStageState(0, D3DTSS_COLOROP, tss0ColorOp, Capture_TSS0ColorOp);
        RestoreTextureStageState(0, D3DTSS_COLORARG1, tss0ColorArg1, Capture_TSS0ColorArg1);
        RestoreTextureStageState(0, D3DTSS_COLORARG2, tss0ColorArg2, Capture_TSS0ColorArg2);
        RestoreTextureStageState(0, D3DTSS_ALPHAOP, tss0AlphaOp, Capture_TSS0AlphaOp);
        RestoreTextureStageState(0, D3DTSS_ALPHAARG1, tss0AlphaArg1, Capture_TSS0AlphaArg1);
        RestoreTextureStageState(0, D3DTSS_ALPHAARG2, tss0AlphaArg2, Capture_TSS0AlphaArg2);
        RestoreTextureStageState(0, D3DTSS_MINFILTER, tss0MinFilter, Capture_TSS0MinFilter);
        RestoreTextureStageState(0, D3DTSS_MAGFILTER, tss0MagFilter, Capture_TSS0MagFilter);
        RestoreTextureStageState(0, D3DTSS_MIPFILTER, tss0MipFilter, Capture_TSS0MipFilter);
        RestoreTextureStageState(0, D3DTSS_ADDRESSU, tss0AddressU, Capture_TSS0AddressU);
        RestoreTextureStageState(0, D3DTSS_ADDRESSV, tss0AddressV, Capture_TSS0AddressV);
        RestoreTextureStageState(0, D3DTSS_TEXCOORDINDEX, tss0TexCoordIndex, Capture_TSS0TexCoordIndex);
        RestoreTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, tss0TextureTransformFlags, Capture_TSS0TextureTransformFlags);
        RestoreTextureStageState(1, D3DTSS_COLOROP, tss1ColorOp, Capture_TSS1ColorOp);
        RestoreTextureStageState(1, D3DTSS_ALPHAOP, tss1AlphaOp, Capture_TSS1AlphaOp);

        RestoreTexture(0, texture0, Capture_Texture0);
        RestoreTexture(1, texture1, Capture_Texture1);

        if ((capturedFlags & Capture_Viewport) && viewport.dwWidth != 0 && viewport.dwHeight != 0) {
            device->SetViewport(&viewport);
        }
    }

    D3D7StateGuard(const D3D7StateGuard&) = delete;
    D3D7StateGuard& operator=(const D3D7StateGuard&) = delete;

private:
    enum CaptureFlag : uint64_t {
        Capture_World = 1ull << 0,
        Capture_View = 1ull << 1,
        Capture_Projection = 1ull << 2,
        Capture_Viewport = 1ull << 3,
        Capture_ZEnable = 1ull << 4,
        Capture_ZWrite = 1ull << 5,
        Capture_ZFunc = 1ull << 6,
        Capture_Lighting = 1ull << 7,
        Capture_ShadeMode = 1ull << 8,
        Capture_FogEnable = 1ull << 9,
        Capture_RangeFogEnable = 1ull << 10,
        Capture_Clipping = 1ull << 11,
        Capture_AlphaBlend = 1ull << 12,
        Capture_AlphaTest = 1ull << 13,
        Capture_AlphaFunc = 1ull << 14,
        Capture_AlphaRef = 1ull << 15,
        Capture_SrcBlend = 1ull << 16,
        Capture_DestBlend = 1ull << 17,
        Capture_CullMode = 1ull << 18,
        Capture_ZBias = 1ull << 19,
        Capture_StencilEnable = 1ull << 20,
        Capture_Texture0 = 1ull << 21,
        Capture_Texture1 = 1ull << 22,
        Capture_TSS0ColorOp = 1ull << 23,
        Capture_TSS0ColorArg1 = 1ull << 24,
        Capture_TSS0ColorArg2 = 1ull << 25,
        Capture_TSS0AlphaOp = 1ull << 26,
        Capture_TSS0AlphaArg1 = 1ull << 27,
        Capture_TSS0AlphaArg2 = 1ull << 28,
        Capture_TSS0MinFilter = 1ull << 29,
        Capture_TSS0MagFilter = 1ull << 30,
        Capture_TSS0MipFilter = 1ull << 31,
        Capture_TSS0AddressU = 1ull << 32,
        Capture_TSS0AddressV = 1ull << 33,
        Capture_TSS0TexCoordIndex = 1ull << 34,
        Capture_TSS0TextureTransformFlags = 1ull << 35,
        Capture_TSS1ColorOp = 1ull << 36,
        Capture_TSS1AlphaOp = 1ull << 37,
    };

    void CaptureTransform(const D3DTRANSFORMSTATETYPE state, D3DMATRIX& value, const CaptureFlag flag) {
        if (SUCCEEDED(device->GetTransform(state, &value))) capturedFlags |= flag;
    }

    void RestoreTransform(const D3DTRANSFORMSTATETYPE state, const D3DMATRIX& value, const CaptureFlag flag) const {
        if (capturedFlags & flag) device->SetTransform(state, const_cast<D3DMATRIX*>(&value));
    }

    void CaptureRenderState(const D3DRENDERSTATETYPE state, DWORD& value, const CaptureFlag flag) {
        if (SUCCEEDED(device->GetRenderState(state, &value))) capturedFlags |= flag;
    }

    void RestoreRenderState(const D3DRENDERSTATETYPE state, const DWORD value, const CaptureFlag flag) const {
        if (capturedFlags & flag) device->SetRenderState(state, value);
    }

    void CaptureTextureStageState(
        const DWORD stage,
        const D3DTEXTURESTAGESTATETYPE state,
        DWORD& value,
        const CaptureFlag flag) {
        if (SUCCEEDED(device->GetTextureStageState(stage, state, &value))) capturedFlags |= flag;
    }

    void RestoreTextureStageState(
        const DWORD stage,
        const D3DTEXTURESTAGESTATETYPE state,
        const DWORD value,
        const CaptureFlag flag) const {
        if (capturedFlags & flag) device->SetTextureStageState(stage, state, value);
    }

    void CaptureTexture(const DWORD stage, IDirectDrawSurface7*& texture, const CaptureFlag flag) {
        if (SUCCEEDED(device->GetTexture(stage, &texture))) capturedFlags |= flag;
    }

    void RestoreTexture(const DWORD stage, IDirectDrawSurface7*& texture, const CaptureFlag flag) const {
        if (capturedFlags & flag) device->SetTexture(stage, texture);
        if (texture) {
            texture->Release();
            texture = nullptr;
        }
    }

private:
    IDirect3DDevice7* device = nullptr;
    uint64_t capturedFlags = 0;
    D3DMATRIX world{};
    D3DMATRIX view{};
    D3DMATRIX projection{};
    D3DVIEWPORT7 viewport{};
    DWORD zEnable{};
    DWORD zWrite{};
    DWORD zFunc{};
    DWORD lighting{};
    DWORD shadeMode{};
    DWORD fogEnable{};
    DWORD rangeFogEnable{};
    DWORD clipping{};
    DWORD alphaBlend{};
    DWORD alphaTest{};
    DWORD alphaFunc{};
    DWORD alphaRef{};
    DWORD srcBlend{};
    DWORD destBlend{};
    DWORD cullMode{};
    DWORD zBias{};
    DWORD stencilEnable{};
    DWORD tss0ColorOp{};
    DWORD tss0ColorArg1{};
    DWORD tss0ColorArg2{};
    DWORD tss0AlphaOp{};
    DWORD tss0AlphaArg1{};
    DWORD tss0AlphaArg2{};
    DWORD tss0MinFilter{};
    DWORD tss0MagFilter{};
    DWORD tss0MipFilter{};
    DWORD tss0AddressU{};
    DWORD tss0AddressV{};
    DWORD tss0TexCoordIndex{};
    DWORD tss0TextureTransformFlags{};
    DWORD tss1ColorOp{};
    DWORD tss1AlphaOp{};
    IDirectDrawSurface7* texture0 = nullptr;
    IDirectDrawSurface7* texture1 = nullptr;
};
