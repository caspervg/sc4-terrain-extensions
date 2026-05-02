#pragma once

#include <d3d.h>
#include <ddraw.h>

struct D3D7StateGuard {
    explicit D3D7StateGuard(IDirect3DDevice7* dev) : device(dev) {
        if (!device) return;
        ok[0]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZENABLE,          &rs[0]));
        ok[1]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     &rs[1]));
        ok[2]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_LIGHTING,         &rs[2]));
        ok[3]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &rs[3]));
        ok[4]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_SRCBLEND,         &rs[4]));
        ok[5]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_DESTBLEND,        &rs[5]));
        ok[6]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_CULLMODE,         &rs[6]));
        ok[7]  = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZBIAS,            &rs[7]));
        ok[8]  = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_COLOROP,   &tss[0]));
        ok[9]  = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_COLORARG1, &tss[1]));
        ok[10] = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_ALPHAOP,   &tss[2]));
        ok[11] = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &tss[3]));
        ok[12] = SUCCEEDED(device->GetTextureStageState(1, D3DTSS_COLOROP,   &tss[4]));
        ok[13] = SUCCEEDED(device->GetTextureStageState(1, D3DTSS_ALPHAOP,   &tss[5]));
        okTex  = SUCCEEDED(device->GetTexture(0, &tex0));
    }

    ~D3D7StateGuard() {
        if (!device) return;
        static constexpr D3DRENDERSTATETYPE kStates[] = {
            D3DRENDERSTATE_ZENABLE, D3DRENDERSTATE_ZWRITEENABLE, D3DRENDERSTATE_LIGHTING,
            D3DRENDERSTATE_ALPHABLENDENABLE, D3DRENDERSTATE_SRCBLEND, D3DRENDERSTATE_DESTBLEND,
            D3DRENDERSTATE_CULLMODE, D3DRENDERSTATE_ZBIAS
        };
        for (int i = 0; i < 8; ++i) {
            if (ok[i]) device->SetRenderState(kStates[i], rs[i]);
        }
        if (ok[8])  device->SetTextureStageState(0, D3DTSS_COLOROP,   tss[0]);
        if (ok[9])  device->SetTextureStageState(0, D3DTSS_COLORARG1, tss[1]);
        if (ok[10]) device->SetTextureStageState(0, D3DTSS_ALPHAOP,   tss[2]);
        if (ok[11]) device->SetTextureStageState(0, D3DTSS_ALPHAARG1, tss[3]);
        if (ok[12]) device->SetTextureStageState(1, D3DTSS_COLOROP,   tss[4]);
        if (ok[13]) device->SetTextureStageState(1, D3DTSS_ALPHAOP,   tss[5]);
        if (okTex) {
            device->SetTexture(0, tex0);
            if (tex0) tex0->Release();
        }
    }

    D3D7StateGuard(const D3D7StateGuard&) = delete;
    D3D7StateGuard& operator=(const D3D7StateGuard&) = delete;

    IDirect3DDevice7* device = nullptr;
    bool ok[14]{};
    DWORD rs[8]{};
    DWORD tss[6]{};
    bool okTex = false;
    IDirectDrawSurface7* tex0 = nullptr;
};