#pragma once

#include <optional>

#include "controls/IControlState.hpp"
#include "../ConstantGradeDragState.hpp"
#include "../ConstantGradeOperation.hpp"

class ConstantGradeOperation;
class ConstantGradeRenderer;
struct ConstantGradeSettings;

class ConstantGradeSelectingState final : public IControlState {
public:
    ConstantGradeSelectingState(
        ConstantGradeSettings& settings,
        ConstantGradeOperation& operation,
        ConstantGradeRenderer& renderer,
        ConstantGradeDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Selecting; }
    const char* GetName() const override { return "ConstantGradeSelecting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;
    void OnExit(StatefulDragViewInputControl& ctrl) override;

    bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;
    bool OnKeyUp(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) override;

private:
    bool RebuildPreview_(StatefulDragViewInputControl& ctrl, uint32_t modifiers);
    bool SyncSnapAngle_(uint32_t modifiers);

    ConstantGradeSettings& settings_;
    ConstantGradeOperation& operation_;
    ConstantGradeRenderer& renderer_;
    ConstantGradeDragState& dragState_;
};
