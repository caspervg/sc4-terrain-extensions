#include "OverlayDrawManager.hpp"

#include <algorithm>

void OverlayDrawManager::Register(OverlayRenderer* renderer) {
	if (renderer && std::find(renderers_.begin(), renderers_.end(), renderer) == renderers_.end()) {
		renderers_.push_back(renderer);
	}
}

void OverlayDrawManager::Unregister(OverlayRenderer* renderer) {
	renderers_.erase(
		std::remove(renderers_.begin(), renderers_.end(), renderer),
		renderers_.end()
	);
}

bool OverlayDrawManager::HasVisibleGeometry() const {
	for (const auto* renderer : renderers_) {
		if (renderer && renderer->HasVisibleGeometry()) {
			return true;
		}
	}

	return false;
}

void OverlayDrawManager::CollectVisibleVertices(std::vector<OverlayVertex>& out) const {
	for (const auto* renderer : renderers_) {
		if (renderer) {
			renderer->AppendVisibleVertices(out);
		}
	}
}

void OverlayDrawManager::DrawAll(IDirect3DDevice7* device) {
	for (auto* renderer : renderers_) {
		if (renderer) {
			renderer->Draw(device);
		}
	}
}
