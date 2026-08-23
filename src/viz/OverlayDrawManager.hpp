#pragma once
#include "OverlayRenderer.hpp"


class OverlayDrawManager {
public:
    void Register(OverlayRenderer* renderer);
    void Unregister(OverlayRenderer* renderer);
    [[nodiscard]] bool HasVisibleGeometry() const;
    void DrawAll(IDirect3DDevice7* device);
    /// Appends vertices of all visible layers of all registered renderers.
    void CollectVisibleVertices(std::vector<OverlayVertex>& out) const;
private:
    std::vector<OverlayRenderer*> renderers_;
};
