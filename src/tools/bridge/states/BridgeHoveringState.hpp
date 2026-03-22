#pragma once

#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"
#include "tools/bridge/BridgeDragState.hpp"

struct BridgeToolSettings;
class BridgeApproachRenderer;

class BridgeHoveringState final : public IControlState {
public:
    BridgeHoveringState(BridgeToolSettings& settings,
                        BridgeApproachRenderer& renderer,
                        BridgeDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Hovering; }
    const char* GetName() const override { return "Hovering"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    void UpdateHintText_(StatefulDragViewInputControl& ctrl, uint32_t modifiers) const;
    void UpdateHoverSelection_(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z) const;

    BridgeToolSettings& settings_;
    BridgeApproachRenderer& renderer_;
    BridgeDragState& dragState_;
};
