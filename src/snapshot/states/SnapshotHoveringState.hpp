#pragma once
#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"

struct SnapshotDragState;
class SnapshotManager;
class SnapshotPreviewRenderer;

class SnapshotHoveringState final : public IControlState {
public:
    SnapshotHoveringState(SnapshotManager& mgr,
                          SnapshotPreviewRenderer& renderer,
                          SnapshotDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Hovering; }
    const char* GetName() const override { return "SnapshotHovering"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    SnapshotManager& mgr_;
    SnapshotPreviewRenderer& renderer_;
    SnapshotDragState& dragState_;
};
