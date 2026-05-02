#pragma once

#include <memory>

#include "controls/ViewInputControlReleaser.hpp"
#include "core/IDragTool.hpp"
#include "ConstantGradeOperation.hpp"
#include "ConstantGradeRenderer.hpp"
#include "ConstantGradeSettings.hpp"

class ConstantGradeInteractiveTool final : public IDragTool {
public:
    ConstantGradeInteractiveTool();
    ~ConstantGradeInteractiveTool() override;

    [[nodiscard]] const char* GetCheatName() const override { return "constantgrade_vic"; }
    [[nodiscard]] const uint32_t GetCheatId() const override { return kActivationId; }

    void Activate(cISC4City* city, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&, SnapshotManager*) override;
    void Deactivate() override;

    StatefulDragViewInputControl* GetInputControl() override;

private:
    static constexpr uint32_t kActivationId = 0x2099E802u;

    ConstantGradeSettings settings_;
    std::unique_ptr<ConstantGradeRenderer> renderer_;
    std::unique_ptr<ConstantGradeOperation> operation_;
    std::unique_ptr<StatefulDragViewInputControl, ViewInputControlReleaser> control_;
    OverlayDrawManager* drawMgr_{nullptr};
    cISC4View3DWin* view3d_{nullptr};
};
