#pragma once

#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"
#include "tools/bridge/IBridgeDragContext.hpp"

struct BridgeToolSettings;
class BridgeApproachRenderer;

class BridgeSelectingState final : public IControlState {
public:
    BridgeSelectingState(BridgeToolSettings& settings,
                         BridgeApproachRenderer& renderer,
                         IBridgeDragContext& context);

    ControlStateId GetStateId() const override { return ControlStateId::Selecting; }
    const char* GetName() const override { return "Selecting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    void RebuildPreview_(StatefulDragViewInputControl& ctrl) const;

    BridgeToolSettings& settings_;
    BridgeApproachRenderer& renderer_;
    IBridgeDragContext& context_;
};
