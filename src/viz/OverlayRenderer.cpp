#include "OverlayRenderer.hpp"

#include <cmath>
#include <d3d.h>
#include <ddraw.h>

void OverlayRenderer::EmitLine(const OverlayVertex& start, const OverlayVertex& end,
                               float thickness, DWORD color, uint32_t layerId) {
	float dx = end.x - start.x;
	float dz = end.z - start.z;
	float len = std::sqrt(dx * dx + dz * dz);

	float px, pz;
	if (len > 0.001f) {
		px = -dz / len * thickness * 0.5f;
		pz =  dx / len * thickness * 0.5f;
	} else {
		px = thickness * 0.5f;
		pz = 0.0f;
	}

	auto& layer = GetOrCreateLayer_(layerId);

	// Two triangles forming a quad
	layer.vertices.push_back({start.x - px, start.y, start.z - pz, color});
	layer.vertices.push_back({start.x + px, start.y, start.z + pz, color});
	layer.vertices.push_back({end.x   + px, end.y,   end.z   + pz, color});

	layer.vertices.push_back({start.x - px, start.y, start.z - pz, color});
	layer.vertices.push_back({end.x   + px, end.y,   end.z   + pz, color});
	layer.vertices.push_back({end.x   - px, end.y,   end.z   - pz, color});
}

void OverlayRenderer::EmitQuad(const OverlayVertex& a, const OverlayVertex& b,
                               const OverlayVertex& c, const OverlayVertex& d,
                               DWORD color, uint32_t layerId) {
	auto& layer = GetOrCreateLayer_(layerId);
	layer.vertices.push_back({a.x, a.y, a.z, color});
	layer.vertices.push_back({b.x, b.y, b.z, color});
	layer.vertices.push_back({c.x, c.y, c.z, color});

	layer.vertices.push_back({a.x, a.y, a.z, color});
	layer.vertices.push_back({c.x, c.y, c.z, color});
	layer.vertices.push_back({d.x, d.y, d.z, color});
}

void OverlayRenderer::Clear() {
	layers_.clear();
}

void OverlayRenderer::ClearLayer(uint32_t layerId) {
	auto it = layers_.find(layerId);
	if (it != layers_.end()) {
		it->second.vertices.clear();
	}
}

void OverlayRenderer::SetLayerVisible(uint32_t layerId, bool visible) {
	GetOrCreateLayer_(layerId).visible = visible;
}

bool OverlayRenderer::HasLayer(uint32_t layerId) const {
	return layers_.count(layerId) > 0;
}

void OverlayRenderer::Draw(IDirect3DDevice7* device) {
	if (!device) return;

	SetupRenderState_(device);

	for (auto& [id, layer] : layers_) {
		if (!layer.visible || layer.vertices.empty()) continue;

		device->DrawPrimitive(
			D3DPT_TRIANGLELIST,
			D3DFVF_XYZ | D3DFVF_DIFFUSE,
			layer.vertices.data(),
			static_cast<DWORD>(layer.vertices.size()),
			0
		);
	}

	RestoreRenderState_(device);
}

OverlayRenderer::Layer& OverlayRenderer::GetOrCreateLayer_(uint32_t layerId) {
	return layers_[layerId];
}

void OverlayRenderer::SetupRenderState_(IDirect3DDevice7* device) {
	savedState_ = {};
	savedState_.okZEnable = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZENABLE, &savedState_.zEnable));
	savedState_.okZWrite = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &savedState_.zWrite));
	savedState_.okZFunc = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZFUNC, &savedState_.zFunc));
	savedState_.okLighting = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_LIGHTING, &savedState_.lighting));
	savedState_.okFogEnable = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_FOGENABLE, &savedState_.fogEnable));
	savedState_.okRangeFogEnable = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_RANGEFOGENABLE, &savedState_.rangeFogEnable));
	savedState_.okAlphaBlend = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &savedState_.alphaBlend));
	savedState_.okAlphaTest = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, &savedState_.alphaTest));
	savedState_.okAlphaFunc = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ALPHAFUNC, &savedState_.alphaFunc));
	savedState_.okAlphaRef = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ALPHAREF, &savedState_.alphaRef));
	savedState_.okStencilEnable = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_STENCILENABLE, &savedState_.stencilEnable));
	savedState_.okSrcBlend = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_SRCBLEND, &savedState_.srcBlend));
	savedState_.okDstBlend = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_DESTBLEND, &savedState_.dstBlend));
	savedState_.okCullMode = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_CULLMODE, &savedState_.cullMode));
	savedState_.okZBias = SUCCEEDED(device->GetRenderState(D3DRENDERSTATE_ZBIAS, &savedState_.zBias));
	savedState_.okTexture0 = SUCCEEDED(device->GetTexture(0, &savedState_.texture0));
	savedState_.okTexture1 = SUCCEEDED(device->GetTexture(1, &savedState_.texture1));
	savedState_.okTss0ColorOp = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_COLOROP, &savedState_.tss0ColorOp));
	savedState_.okTss0ColorArg1 = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_COLORARG1, &savedState_.tss0ColorArg1));
	savedState_.okTss0AlphaOp = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_ALPHAOP, &savedState_.tss0AlphaOp));
	savedState_.okTss0AlphaArg1 = SUCCEEDED(device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &savedState_.tss0AlphaArg1));
	savedState_.okTss1ColorOp = SUCCEEDED(device->GetTextureStageState(1, D3DTSS_COLOROP, &savedState_.tss1ColorOp));
	savedState_.okTss1AlphaOp = SUCCEEDED(device->GetTextureStageState(1, D3DTSS_ALPHAOP, &savedState_.tss1AlphaOp));

	device->SetRenderState(D3DRENDERSTATE_ZENABLE, TRUE);
	device->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
	device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
	device->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE);
	device->SetRenderState(D3DRENDERSTATE_RANGEFOGENABLE, TRUE);
	device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_ALWAYS);
	device->SetRenderState(D3DRENDERSTATE_ALPHAREF, 0);
	device->SetRenderState(D3DRENDERSTATE_STENCILENABLE, FALSE);
	device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRENDERSTATE_ZBIAS, 1);
	device->SetTexture(0, nullptr);
	device->SetTexture(1, nullptr);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

void OverlayRenderer::RestoreRenderState_(IDirect3DDevice7* device) {
	if (savedState_.okZEnable) device->SetRenderState(D3DRENDERSTATE_ZENABLE, savedState_.zEnable);
	if (savedState_.okZWrite) device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, savedState_.zWrite);
	if (savedState_.okZFunc) device->SetRenderState(D3DRENDERSTATE_ZFUNC, savedState_.zFunc);
	if (savedState_.okLighting) device->SetRenderState(D3DRENDERSTATE_LIGHTING, savedState_.lighting);
	if (savedState_.okFogEnable) device->SetRenderState(D3DRENDERSTATE_FOGENABLE, savedState_.fogEnable);
	if (savedState_.okRangeFogEnable) device->SetRenderState(D3DRENDERSTATE_RANGEFOGENABLE, savedState_.rangeFogEnable);
	if (savedState_.okAlphaBlend) device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, savedState_.alphaBlend);
	if (savedState_.okAlphaTest) device->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, savedState_.alphaTest);
	if (savedState_.okAlphaFunc) device->SetRenderState(D3DRENDERSTATE_ALPHAFUNC, savedState_.alphaFunc);
	if (savedState_.okAlphaRef) device->SetRenderState(D3DRENDERSTATE_ALPHAREF, savedState_.alphaRef);
	if (savedState_.okStencilEnable) device->SetRenderState(D3DRENDERSTATE_STENCILENABLE, savedState_.stencilEnable);
	if (savedState_.okSrcBlend) device->SetRenderState(D3DRENDERSTATE_SRCBLEND, savedState_.srcBlend);
	if (savedState_.okDstBlend) device->SetRenderState(D3DRENDERSTATE_DESTBLEND, savedState_.dstBlend);
	if (savedState_.okCullMode) device->SetRenderState(D3DRENDERSTATE_CULLMODE, savedState_.cullMode);
	if (savedState_.okZBias) device->SetRenderState(D3DRENDERSTATE_ZBIAS, savedState_.zBias);
	if (savedState_.okTss0ColorOp) device->SetTextureStageState(0, D3DTSS_COLOROP, savedState_.tss0ColorOp);
	if (savedState_.okTss0ColorArg1) device->SetTextureStageState(0, D3DTSS_COLORARG1, savedState_.tss0ColorArg1);
	if (savedState_.okTss0AlphaOp) device->SetTextureStageState(0, D3DTSS_ALPHAOP, savedState_.tss0AlphaOp);
	if (savedState_.okTss0AlphaArg1) device->SetTextureStageState(0, D3DTSS_ALPHAARG1, savedState_.tss0AlphaArg1);
	if (savedState_.okTss1ColorOp) device->SetTextureStageState(1, D3DTSS_COLOROP, savedState_.tss1ColorOp);
	if (savedState_.okTss1AlphaOp) device->SetTextureStageState(1, D3DTSS_ALPHAOP, savedState_.tss1AlphaOp);
	if (savedState_.okTexture0) device->SetTexture(0, savedState_.texture0);
	if (savedState_.okTexture1) device->SetTexture(1, savedState_.texture1);
	if (savedState_.texture0) {
		savedState_.texture0->Release();
		savedState_.texture0 = nullptr;
	}
	if (savedState_.texture1) {
		savedState_.texture1->Release();
		savedState_.texture1 = nullptr;
	}
}
