#pragma once
#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"

struct SnapshotDragState;
class SnapshotManager;
class SnapshotPreviewRenderer;

class SnapshotSelectingState final : public IControlState {
public:
    SnapshotSelectingState(SnapshotManager& mgr,
                           SnapshotPreviewRenderer& renderer,
                           SnapshotDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Selecting; }
    const char* GetName() const override { return "SnapshotSelecting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    void UpdateSelection_(StatefulDragViewInputControl& ctrl);

    SnapshotManager& mgr_;
    SnapshotPreviewRenderer& renderer_;
    SnapshotDragState& dragState_;
};
