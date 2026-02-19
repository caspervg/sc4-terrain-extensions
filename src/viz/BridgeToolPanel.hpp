#pragma once
#include "public/ImGuiPanel.h"

struct BridgeToolSettings;

static constexpr auto kBridgeToolPanelId = 0x5489af52u;

static auto gShowHeightMarkers = true;
static auto gShowGradeColors = true;
static auto gShowGrid = true;

class BridgeToolPanel final : public ImGuiPanel {
public:
    explicit BridgeToolPanel(BridgeToolSettings& settings);

    void OnRender() override;
    void SetOpen(bool open);

private:
    BridgeToolSettings& settings_;
    bool bOpen;
};
