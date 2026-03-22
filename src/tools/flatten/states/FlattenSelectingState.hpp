#pragma once

#include "controls/IControlState.hpp"

#include "../FlattenDragState.hpp"
#include "../FlattenOperation.hpp"
#include "../FlattenRenderer.hpp"
#include "../FlattenSettings.hpp"

class FlattenSelectingState final : public IControlState {
public:
    FlattenSelectingState(FlattenSettings& settings, FlattenOperation& operation, FlattenRenderer& renderer,
                          FlattenDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Selecting; }
    const char* GetName() const override { return "FlattenSelecting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    bool RebuildPreview_(StatefulDragViewInputControl& ctrl, uint32_t modifiers) const;
    FlattenRequest BuildRequest_() const;
    void UpdateCursor_(StatefulDragViewInputControl& ctrl, const FlattenPreview& preview, uint32_t modifiers) const;
    bool HandleAdjustment_(StatefulDragViewInputControl& ctrl, uint32_t modifiers, int32_t delta) const;

private:
    FlattenSettings& settings_;
    FlattenOperation& operation_;
    FlattenRenderer& renderer_;
    FlattenDragState& dragState_;
};
