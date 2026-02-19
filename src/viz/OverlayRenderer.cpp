#include "OverlayRenderer.hpp"

#include <cmath>
#include <d3d.h>

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
	device->GetRenderState(D3DRENDERSTATE_ZENABLE, &savedState_.zEnable);
	device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &savedState_.zWrite);
	device->GetRenderState(D3DRENDERSTATE_LIGHTING, &savedState_.lighting);
	device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &savedState_.alphaBlend);
	device->GetRenderState(D3DRENDERSTATE_CULLMODE, &savedState_.cullMode);
	device->GetRenderState(D3DRENDERSTATE_ZBIAS, &savedState_.zBias);

	device->SetRenderState(D3DRENDERSTATE_ZENABLE, FALSE);
	device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
	device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRENDERSTATE_ZBIAS, 8);
	device->SetTexture(0, nullptr);
}

void OverlayRenderer::RestoreRenderState_(IDirect3DDevice7* device) {
	device->SetRenderState(D3DRENDERSTATE_ZENABLE, savedState_.zEnable);
	device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, savedState_.zWrite);
	device->SetRenderState(D3DRENDERSTATE_LIGHTING, savedState_.lighting);
	device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, savedState_.alphaBlend);
	device->SetRenderState(D3DRENDERSTATE_CULLMODE, savedState_.cullMode);
	device->SetRenderState(D3DRENDERSTATE_ZBIAS, savedState_.zBias);
}