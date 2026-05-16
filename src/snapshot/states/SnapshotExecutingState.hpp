#pragma once
#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"

struct SnapshotDragState;
class SnapshotManager;
class SnapshotPreviewRenderer;

class SnapshotExecutingState final : public IControlState {
public:
    SnapshotExecutingState(SnapshotManager& mgr,
                           SnapshotPreviewRenderer& renderer,
                           SnapshotDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Executing; }
    const char* GetName() const override { return "SnapshotExecuting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;

private:
    SnapshotManager& mgr_;
    SnapshotPreviewRenderer& renderer_;
    SnapshotDragState& dragState_;
};
