#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "public/SC4WorldMeshBuilder.h"
#include "public/cIGZWorldDrawService.h"

struct OverlayVertex {
    float x, y, z;
    uint32_t color;
};

// Geometry handed to cIGZWorldDrawService layers, which draw it every frame on any graphics driver
// until it changes. Emit/Clear calls only change the local copy; Upload() hands changed layers to
// the service. Without a service the renderer keeps its geometry and draws nothing.
class OverlayRenderer {
public:
    OverlayRenderer() = default;
    virtual ~OverlayRenderer();
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    // Starts drawing through this service, or stops drawing with nullptr. Removes the layers made
    // on the previous service, so call it with nullptr before that service goes away.
    void SetWorldDrawService(cIGZWorldDrawService* service);
    void Upload();

    // A flat band `widthMeters` wide; vertical bands become two crossed bands.
    void EmitLine(const OverlayVertex& start, const OverlayVertex& end, float widthMeters, uint32_t color, uint32_t layerId);
    // A line that keeps its width in pixels on screen at every zoom level and faces the camera.
    void EmitScreenLine(const OverlayVertex& start, const OverlayVertex& end, float widthPixels, uint32_t color, uint32_t layerId);
    void EmitQuad(const OverlayVertex& a, const OverlayVertex& b, const OverlayVertex& c, const OverlayVertex& d, uint32_t color, uint32_t layerId);
    void ClearLayer(uint32_t layerId);

protected:
    // Layers default to kOnTopStyle in the PostDynamic pass, like the old D3D7 overlay.
    void ConfigureLayer(uint32_t layerId, DrawServicePass pass, const SC4WorldStyle& style);

    static constexpr SC4WorldStyle kOnTopStyle{.depth = SC4WorldDepth::AlwaysOnTop};
    static constexpr SC4WorldStyle kMapStyle{.depth = SC4WorldDepth::AlwaysOnTop, .fadeWithFog = true};

private:
    struct ScreenLine {
        SC4WorldLineStyle style;
        std::vector<SC4WorldVertex> points;
    };
    struct Layer {
        SC4WorldMeshBuilder mesh;
        std::vector<ScreenLine> lines;
        DrawServicePass pass = DrawServicePass::PostDynamic;
        SC4WorldStyle style = kOnTopStyle;
        uint32_t id = 0;
        bool dirty = false;
    };

    void DestroyLayers_();

    std::unordered_map<uint32_t, Layer> layers_;
    cIGZWorldDrawService* worldDraw_ = nullptr;
};
