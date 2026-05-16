#pragma once
#include <memory>

#include "BridgeToolSettings.hpp"
#include "controls/ViewInputControlReleaser.hpp"
#include "core/IDragTool.hpp"
#include "viz/BridgeApproachRenderer.hpp"
#include "cRZAutoRefCount.h"
#include "public/cIGZImGuiService.h"

class BridgeApproachDragTool final : public IDragTool {
public:
    BridgeApproachDragTool();
    ~BridgeApproachDragTool() override;

    [[nodiscard]] const char* GetCheatName() const override { return "bridgebuilder"; }
    [[nodiscard]] const uint32_t GetCheatId() const override { return kCheatId; }

    void Activate(cISC4City* city, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&, SnapshotManager*) override;
    void Deactivate() override;

    StatefulDragViewInputControl* GetInputControl() override;
    [[nodiscard]] const BridgeToolSettings& GetSettings() const;

private:
    static constexpr auto kCheatId{0x9773F4CDu};

    BridgeToolSettings settings_;

    std::unique_ptr<BridgeApproachRenderer> renderer_;
    std::unique_ptr<StatefulDragViewInputControl, ViewInputControlReleaser> control_;
    cRZAutoRefCount<cIGZImGuiService> imguiService_;
    OverlayDrawManager* drawMgr_{nullptr};
    cISC4View3DWin* view3d_{nullptr};

    bool panelRegistered_{false};
};
