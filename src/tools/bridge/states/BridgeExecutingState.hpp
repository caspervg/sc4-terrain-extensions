#pragma once

#include "controls/IControlState.hpp"
#include "controls/ControlStateId.hpp"

class IBridgeDragContext;
struct BridgeToolSettings;
class BridgeApproachRenderer;

class BridgeExecutingState final : public IControlState {
public:
    explicit BridgeExecutingState(BridgeToolSettings& settings,
                                  IBridgeDragContext& context);

    ControlStateId GetStateId() const override { return ControlStateId::Executing; }
    const char* GetName() const override { return "Executing"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;

private:
    BridgeToolSettings& settings_;
    IBridgeDragContext& context_;
};
