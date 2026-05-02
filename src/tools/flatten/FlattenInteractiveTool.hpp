#pragma once

#include <memory>

#include "controls/ViewInputControlReleaser.hpp"
#include "core/IDragTool.hpp"
#include "FlattenDragState.hpp"
#include "FlattenOperation.hpp"
#include "FlattenRenderer.hpp"
#include "FlattenSettings.hpp"

class FlattenInteractiveTool final : public IDragTool {
public:
    FlattenInteractiveTool();
    ~FlattenInteractiveTool() override;

    [[nodiscard]] const char* GetCheatName() const override { return "flatten_vic"; }
    [[nodiscard]] const uint32_t GetCheatId() const override { return kActivationId; }

    void Activate(cISC4City* city, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&, SnapshotManager*) override;
    void Deactivate() override;

    StatefulDragViewInputControl* GetInputControl() override;

private:
    static constexpr uint32_t kActivationId = 0x2099E801u;

    FlattenSettings settings_;
    std::unique_ptr<FlattenRenderer> renderer_;
    std::unique_ptr<FlattenOperation> operation_;
    std::unique_ptr<StatefulDragViewInputControl, ViewInputControlReleaser> control_;
    OverlayDrawManager* drawMgr_{nullptr};
    cISC4View3DWin* view3d_{nullptr};
};
