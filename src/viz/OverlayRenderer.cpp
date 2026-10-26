#include "OverlayRenderer.hpp"

OverlayRenderer::~OverlayRenderer() {
	DestroyLayers_();
}

void OverlayRenderer::SetWorldDrawService(cIGZWorldDrawService* service) {
	if (service == worldDraw_) return;

	DestroyLayers_();
	worldDraw_ = service;
	for (auto& [id, layer] : layers_) {
		layer.dirty = true;
	}
	Upload();
}

void OverlayRenderer::Upload() {
	if (!worldDraw_) return;

	for (auto& [layerId, layer] : layers_) {
		if (!layer.dirty) continue;

		if (layer.id == 0) {
			if (layer.mesh.Empty() && layer.lines.empty()) {
				layer.dirty = false;
				continue;
			}
			layer.id = worldDraw_->CreateLayer(layer.pass, layer.style, static_cast<int32_t>(layerId));
			if (layer.id == 0) {
				// The service cannot draw (yet); keep the geometry and try again next time.
				continue;
			}
		}

		worldDraw_->SetLayerTriangles(layer.id, layer.mesh.Data(), layer.mesh.Count());
		for (const auto& line : layer.lines) {
			worldDraw_->AddLayerLine(layer.id, line.style, line.points.data(), static_cast<uint32_t>(line.points.size()));
		}
		layer.dirty = false;
	}
}

void OverlayRenderer::EmitLine(const OverlayVertex& start, const OverlayVertex& end,
                               const float widthMeters, const uint32_t color, const uint32_t layerId) {
	auto& layer = layers_[layerId];
	layer.mesh.AddLine(start.x, start.y, start.z, end.x, end.y, end.z, widthMeters, color);
	layer.dirty = true;
}

void OverlayRenderer::EmitScreenLine(const OverlayVertex& start, const OverlayVertex& end,
                                     const float widthPixels, const uint32_t color, const uint32_t layerId) {
	auto& layer = layers_[layerId];
	layer.dirty = true;

	// Chain onto the previous line when it ends here in the same width and colour, so rails and
	// outlines become a few long lines instead of many short ones.
	if (!layer.lines.empty()) {
		auto& last = layer.lines.back();
		const auto& tail = last.points.back();
		if (last.style.width == widthPixels && tail.color == color
			&& tail.x == start.x && tail.y == start.y && tail.z == start.z) {
			last.points.push_back({end.x, end.y, end.z, color, 0.0f, 0.0f});
			return;
		}
	}

	layer.lines.push_back({
		SC4WorldLineStyle{.width = widthPixels},
		{{start.x, start.y, start.z, color, 0.0f, 0.0f}, {end.x, end.y, end.z, color, 0.0f, 0.0f}}
	});
}

void OverlayRenderer::EmitQuad(const OverlayVertex& a, const OverlayVertex& b,
                               const OverlayVertex& c, const OverlayVertex& d,
                               const uint32_t color, const uint32_t layerId) {
	auto& layer = layers_[layerId];
	layer.mesh.AddQuad({a.x, a.y, a.z, color, 0.0f, 0.0f},
	                   {b.x, b.y, b.z, color, 0.0f, 0.0f},
	                   {c.x, c.y, c.z, color, 0.0f, 0.0f},
	                   {d.x, d.y, d.z, color, 0.0f, 0.0f});
	layer.dirty = true;
}

void OverlayRenderer::ClearLayer(const uint32_t layerId) {
	const auto it = layers_.find(layerId);
	if (it != layers_.end() && (!it->second.mesh.Empty() || !it->second.lines.empty())) {
		it->second.mesh.Clear();
		it->second.lines.clear();
		it->second.dirty = true;
	}
}

void OverlayRenderer::ConfigureLayer(const uint32_t layerId, const DrawServicePass pass, const SC4WorldStyle& style) {
	auto& layer = layers_[layerId];
	layer.pass = pass;
	layer.style = style;
	if (layer.id != 0 && worldDraw_) {
		worldDraw_->SetLayerStyle(layer.id, style);
	}
}

void OverlayRenderer::DestroyLayers_() {
	for (auto& [layerId, layer] : layers_) {
		if (layer.id != 0 && worldDraw_) {
			worldDraw_->DestroyLayer(layer.id);
		}
		layer.id = 0;
	}
}
