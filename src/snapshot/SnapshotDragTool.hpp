#pragma once
#include <memory>

#include "controls/ViewInputControlReleaser.hpp"
#include "core/IDragTool.hpp"
#include "SnapshotDragState.hpp"
#include "SnapshotPreviewRenderer.hpp"
#include "cRZAutoRefCount.h"

class SnapshotManager;

class SnapshotDragTool final : public IDragTool {
public:
    explicit SnapshotDragTool(SnapshotManager& mgr, SnapshotPreviewRenderer& renderer);
    ~SnapshotDragTool() override;

    [[nodiscard]] const char* GetCheatName() const override { return "terrainsnap_restore"; }
    [[nodiscard]] const uint32_t GetCheatId() const override { return kCheatId; }

    void Activate(cISC4City* city, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&, SnapshotManager*) override;
    void Deactivate() override;

    StatefulDragViewInputControl* GetInputControl() override;

    void SetRestoreIndex(int index);

    // Activate without going through cheat system (called from panel)
    void ActivateDirect(cISC4City* city, cISC4View3DWin* view3d, cIGZWinMgr* winMgr,
                        OverlayDrawManager& drawMgr);

private:
    static constexpr uint32_t kCheatId = 0x7E5A9B02;

    SnapshotManager& mgr_;
    SnapshotPreviewRenderer& renderer_;
    SnapshotDragState dragState_;

    std::unique_ptr<StatefulDragViewInputControl, ViewInputControlReleaser> control_;
    OverlayDrawManager* drawMgr_{nullptr};
    cISC4View3DWin* view3d_{nullptr};
};
