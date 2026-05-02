#pragma once
#include "OverlayRenderer.hpp"


class OverlayDrawManager {
public:
    void Register(OverlayRenderer* renderer);
    void Unregister(OverlayRenderer* renderer);
    [[nodiscard]] bool HasVisibleGeometry() const;
    void DrawAll(IDirect3DDevice7* device);
private:
    std::vector<OverlayRenderer*> renderers_;
};
