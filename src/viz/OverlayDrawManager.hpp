#pragma once
#include "OverlayRenderer.hpp"


class OverlayDrawManager {
public:
    // Registered renderers draw through this service; nullptr stops all drawing.
    void SetWorldDrawService(cIGZWorldDrawService* service);
    void Register(OverlayRenderer* renderer);
    void Unregister(OverlayRenderer* renderer);
    void UploadAll();
private:
    std::vector<OverlayRenderer*> renderers_;
    cIGZWorldDrawService* worldDraw_ = nullptr;
};
