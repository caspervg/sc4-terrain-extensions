#pragma once

#include <optional>

#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"
#include "tools/bridge/BridgeDragState.hpp"

struct BridgeToolSettings;
class BridgeApproachRenderer;
struct BridgePlacement;

class BridgeSelectingState final : public IControlState {
public:
    BridgeSelectingState(BridgeToolSettings& settings,
                         BridgeApproachRenderer& renderer,
                         BridgeDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Selecting; }
    const char* GetName() const override { return "Selecting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    void RebuildPreview_(StatefulDragViewInputControl& ctrl, uint32_t modifiers);
    std::optional<BridgePlacement> ResolvePlacementFromDrag_() const;

    BridgeToolSettings& settings_;
    BridgeApproachRenderer& renderer_;
    BridgeDragState& dragState_;
};
