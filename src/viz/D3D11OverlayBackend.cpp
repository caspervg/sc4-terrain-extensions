#include "D3D11OverlayBackend.hpp"

#include "OverlayDrawManager.hpp"
#include "OverlayRenderer.hpp"
#include "public/cIGZS3DCameraService.h"
#include "utils/Logger.h"

#include <d3d11.h>
#include <d3dcompiler.h>

namespace {

struct Vertex {
    float x, y, z;
    std::uint32_t color; // D3DCOLOR (ARGB) read as B8G8R8A8_UNORM
};

constexpr char kShaderSource[] = R"(
cbuffer ViewProjCB : register(b0) { row_major float4x4 gViewProj; };
struct VSIn { float3 pos : POSITION; float4 color : COLOR0; };
struct PSIn { float4 pos : SV_Position; float4 color : COLOR0; };
PSIn vs(VSIn i) {
    PSIn o;
    o.pos = mul(float4(i.pos, 1.0), gViewProj);
    o.color = i.color;
    return o;
}
float4 ps(PSIn i) : SV_Target { return i.color; }
)";

// Lazily-created D3D11 pipeline objects, rebuilt if the device changes
// (e.g. after SCGL device loss / regeneration).
struct Pipeline {
    ID3D11Device* device = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11Buffer* constantBuffer = nullptr; // 16 floats, VP matrix
    ID3D11Buffer* vertexBuffer = nullptr;   // dynamic, grows as needed
    std::size_t vertexCapacity = 0;
    ID3D11RasterizerState* rasterizer = nullptr;
    ID3D11BlendState* blend = nullptr;

    void Release() {
        if (vs) vs->Release();
        if (ps) ps->Release();
        if (layout) layout->Release();
        if (constantBuffer) constantBuffer->Release();
        if (vertexBuffer) vertexBuffer->Release();
        if (rasterizer) rasterizer->Release();
        if (blend) blend->Release();
        *this = {};
    }
};

Pipeline g_pipeline;

bool CreatePipeline(ID3D11Device* device) {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errors = nullptr;

    auto hr = D3DCompile(kShaderSource, sizeof(kShaderSource), nullptr, nullptr,
                         nullptr, "vs", "vs_5_0", 0, 0, &vsBlob, &errors);
    if (FAILED(hr)) {
        if (errors) {
            LOG_ERROR("D3D11OverlayBackend: VS compile failed: {}",
                      static_cast<const char*>(errors->GetBufferPointer()));
            errors->Release();
        }
        return false;
    }
    hr = D3DCompile(kShaderSource, sizeof(kShaderSource), nullptr, nullptr,
                    nullptr, "ps", "ps_5_0", 0, 0, &psBlob, &errors);
    if (FAILED(hr)) {
        if (errors) {
            LOG_ERROR("D3D11OverlayBackend: PS compile failed: {}",
                      static_cast<const char*>(errors->GetBufferPointer()));
            errors->Release();
        }
        vsBlob->Release();
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(), nullptr, &g_pipeline.vs);
    if (SUCCEEDED(hr)) {
        hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                       psBlob->GetBufferSize(), nullptr, &g_pipeline.ps);
    }

    constexpr D3D11_INPUT_ELEMENT_DESC kLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 12,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (SUCCEEDED(hr)) {
        hr = device->CreateInputLayout(kLayoutDesc, 2, vsBlob->GetBufferPointer(),
                                       vsBlob->GetBufferSize(), &g_pipeline.layout);
    }
    vsBlob->Release();
    psBlob->Release();

    if (SUCCEEDED(hr)) {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(float) * 16;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&desc, nullptr, &g_pipeline.constantBuffer);
    }

    // Overlay triangle windings are arbitrary (DX7 path drew with CULL_NONE).
    if (SUCCEEDED(hr)) {
        D3D11_RASTERIZER_DESC rs{};
        rs.FillMode = D3D11_FILL_SOLID;
        rs.CullMode = D3D11_CULL_NONE;
        hr = device->CreateRasterizerState(&rs, &g_pipeline.rasterizer);
    }

    if (SUCCEEDED(hr)) {
        D3D11_BLEND_DESC bs{};
        bs.RenderTarget[0].BlendEnable = TRUE;
        bs.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bs.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bs.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bs.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bs.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bs.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bs.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState(&bs, &g_pipeline.blend);
    }

    if (FAILED(hr)) {
        LOG_ERROR("D3D11OverlayBackend: pipeline creation failed (hr=0x{:08X})", hr);
        g_pipeline.Release();
        return false;
    }

    g_pipeline.device = device;
    LOG_INFO("D3D11OverlayBackend: pipeline created");
    return true;
}

bool EnsureVertexBuffer(ID3D11Device* device, std::size_t vertexCount) {
    if (g_pipeline.vertexBuffer && vertexCount <= g_pipeline.vertexCapacity) {
        return true;
    }

    // ponytail: grow-by-doubling only, no ring buffering; fine for overlay-sized batches.
    std::size_t capacity = g_pipeline.vertexCapacity ? g_pipeline.vertexCapacity : 4096;
    while (capacity < vertexCount) capacity *= 2;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(capacity * sizeof(Vertex));
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* buffer = nullptr;
    const auto hr = device->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr)) {
        LOG_ERROR("D3D11OverlayBackend: vertex buffer creation failed (hr=0x{:08X})", hr);
        return false;
    }

    if (g_pipeline.vertexBuffer) g_pipeline.vertexBuffer->Release();
    g_pipeline.vertexBuffer = buffer;
    g_pipeline.vertexCapacity = capacity;
    return true;
}

/// Builds the combined view-projection matrix for the active renderer camera.
///
/// Layouts verified in the Windows 1.1.641 binary:
/// - cS3DCamera::ViewXform  (0x7FFED0): transform at camera + 0x60
///     - rotation 3x3 (row-major) at transform + 0x04
///     - translation at transform + 0x28, uniform scale at transform + 0x34
/// - cS3DCamera::ProjectionMatrix (0x7FFEF0): 16 floats (row-major, row-vector
///   convention) at camera + 0x9c
/// Matches cS3DCamera::Project (0x7FFF10), which maps clip NDC to screen with
/// y flipped exactly like a D3D11 viewport, so clip coords can be passed through.
bool BuildViewProj(cIGZS3DCameraService* cameras, float outVP[16]) {
    const S3DCameraHandle handle = cameras->WrapActiveRendererCamera();
    if (!handle.ptr) {
        return false;
    }

    auto* cam = static_cast<std::uint8_t*>(handle.ptr);
    const float* r = reinterpret_cast<const float*>(cam + 0x60 + 0x04);
    const float* t = reinterpret_cast<const float*>(cam + 0x60 + 0x28);
    const float s = *reinterpret_cast<const float*>(cam + 0x60 + 0x34);
    const float* p = reinterpret_cast<const float*>(cam + 0x9c);

    // View matrix (row-vector convention): v' = ((v * R) * scale) + translation
    float view[16] = {
        s * r[0], s * r[1], s * r[2], 0.0f,
        s * r[3], s * r[4], s * r[5], 0.0f,
        s * r[6], s * r[7], s * r[8], 0.0f,
        t[0], t[1], t[2], 1.0f,
    };

    // outVP = view * proj (both row-major)
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += view[row * 4 + k] * p[k * 4 + col];
            }
            outVP[row * 4 + col] = sum;
        }
    }
    return true;
}

} // namespace

namespace d3d11overlay {

void DrawFrame(
    ID3D11Device* const device,
    ID3D11DeviceContext* const context,
    IDXGISwapChain* const swapChain,
    ID3D11RenderTargetView* const renderTargetView,
    OverlayDrawManager& overlays,
    cIGZS3DCameraService* const cameraService)
{
    if (!device || !context || !swapChain || !renderTargetView || !cameraService) {
        return;
    }

    std::vector<OverlayVertex> vertices;
    overlays.CollectVisibleVertices(vertices);
    if (vertices.empty()) {
        return;
    }

    if (g_pipeline.device != device) {
        g_pipeline.Release();
        if (!CreatePipeline(device)) {
            return;
        }
    }

    float vp[16];
    if (!BuildViewProj(cameraService, vp)) {
        return;
    }

    if (!EnsureVertexBuffer(device, vertices.size())) {
        return;
    }

    // Upload vertices
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(g_pipeline.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD,
                            0, &mapped))) {
        return;
    }
    auto* dst = static_cast<Vertex*>(mapped.pData);
    for (const auto& v : vertices) {
        *dst++ = Vertex{v.x, v.y, v.z, v.color};
    }
    context->Unmap(g_pipeline.vertexBuffer, 0);

    // Update constants
    if (FAILED(context->Map(g_pipeline.constantBuffer, 0, D3D11_MAP_WRITE_DISCARD,
                            0, &mapped))) {
        return;
    }
    memcpy(mapped.pData, vp, sizeof(vp));
    context->Unmap(g_pipeline.constantBuffer, 0);

    // State: alpha blend, no culling, no depth (matches the DX7 overlay render state).
    constexpr FLOAT kBlendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(g_pipeline.blend, kBlendFactor, 0xFFFFFFFFu);
    context->RSSetState(g_pipeline.rasterizer);

    context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    D3D11_TEXTURE2D_DESC backBufferDesc{};
    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(&backBuffer)))) {
        LOG_WARN("D3D11OverlayBackend: failed to query swap chain back buffer");
        return;
    }
    backBuffer->GetDesc(&backBufferDesc);
    backBuffer->Release();
    if (backBufferDesc.Width == 0 || backBufferDesc.Height == 0) {
        return;
    }
    D3D11_VIEWPORT viewport{0.0f, 0.0f,
                            static_cast<FLOAT>(backBufferDesc.Width),
                            static_cast<FLOAT>(backBufferDesc.Height), 0.0f, 1.0f};
    context->RSSetViewports(1, &viewport);

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    ID3D11Buffer* vb = g_pipeline.vertexBuffer;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetInputLayout(g_pipeline.layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_pipeline.vs, nullptr, 0);
    context->PSSetShader(g_pipeline.ps, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* cb = g_pipeline.constantBuffer;
    context->VSSetConstantBuffers(0, 1, &cb);

    context->Draw(static_cast<UINT>(vertices.size()), 0);

    static bool sLoggedOnce = false;
    if (!sLoggedOnce) {
        sLoggedOnce = true;
        LOG_DEBUG(
            "D3D11OverlayBackend: first draw vertices={} vp00={} vp33={}",
            vertices.size(), vp[0], vp[15]);
    }
}

} // namespace d3d11overlay
