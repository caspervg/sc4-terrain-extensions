#pragma once

#include "controls/IControlState.hpp"

#include "../FlattenDragState.hpp"
#include "../FlattenRenderer.hpp"

class FlattenPickingHeightState final : public IControlState {
public:
    FlattenPickingHeightState(FlattenRenderer& renderer, FlattenDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::ToolSpecific; }
    const char* GetName() const override { return "FlattenPickingReference"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    void UpdateHover_(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z) const;
    void UpdateHintText_(StatefulDragViewInputControl& ctrl, bool hasHoverTile, int32_t tileX, int32_t tileZ) const;

private:
    FlattenRenderer& renderer_;
    FlattenDragState& dragState_;
};
