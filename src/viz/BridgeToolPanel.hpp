#pragma once
#include "public/ImGuiPanel.h"
#include "TerrainExtensionsDllDirector.hpp"

static constexpr auto kBridgeToolPanelId = 0x5489af52u;

static auto gShowHeightMarkers = true;
static auto gShowGradeColors = true;
static auto gShowGrid = true;

class BridgeToolPanel final : public ImGuiPanel {
public:
    explicit BridgeToolPanel(TerrainExtensionsDllDirector* director, cIGZImGuiService* imgui);

    void OnRender() override;
    void SetOpen(bool open);

private:
    TerrainExtensionsDllDirector* pDirector;
    cIGZImGuiService* pImguiService;
    bool bOpen;
};
