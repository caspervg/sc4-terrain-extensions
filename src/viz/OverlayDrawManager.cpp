#include "OverlayDrawManager.hpp"

#include <algorithm>

void OverlayDrawManager::Register(OverlayRenderer* renderer) {
	if (renderer) {
		renderers_.push_back(renderer);
	}
}

void OverlayDrawManager::Unregister(OverlayRenderer* renderer) {
	renderers_.erase(
		std::remove(renderers_.begin(), renderers_.end(), renderer),
		renderers_.end()
	);
}

void OverlayDrawManager::DrawAll(IDirect3DDevice7* device) {
	for (auto* renderer : renderers_) {
		renderer->Draw(device);
	}
}