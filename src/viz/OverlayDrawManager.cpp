#include "OverlayDrawManager.hpp"

#include <algorithm>

void OverlayDrawManager::SetWorldDrawService(cIGZWorldDrawService* service) {
	worldDraw_ = service;
	for (auto* renderer : renderers_) {
		renderer->SetWorldDrawService(service);
	}
}

void OverlayDrawManager::Register(OverlayRenderer* renderer) {
	if (renderer && std::find(renderers_.begin(), renderers_.end(), renderer) == renderers_.end()) {
		renderers_.push_back(renderer);
		renderer->SetWorldDrawService(worldDraw_);
	}
}

void OverlayDrawManager::Unregister(OverlayRenderer* renderer) {
	const auto it = std::find(renderers_.begin(), renderers_.end(), renderer);
	if (it != renderers_.end()) {
		(*it)->SetWorldDrawService(nullptr);
		renderers_.erase(it);
	}
}

void OverlayDrawManager::UploadAll() {
	for (auto* renderer : renderers_) {
		renderer->Upload();
	}
}
