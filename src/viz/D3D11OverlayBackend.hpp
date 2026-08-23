#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
class cIGZS3DCameraService;
class OverlayDrawManager;

/// Draws all visible overlay geometry with raw D3D11 (SCGL-D3D11 backend).
/// Call from an ImGui QueueRender callback on the render thread only.
namespace d3d11overlay {
void DrawFrame(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    IDXGISwapChain* swapChain,
    ID3D11RenderTargetView* renderTargetView,
    OverlayDrawManager& overlays,
    cIGZS3DCameraService* cameraService);
}
