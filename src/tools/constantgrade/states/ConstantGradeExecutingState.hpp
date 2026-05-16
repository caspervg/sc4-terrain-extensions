#pragma once

#include "controls/IControlState.hpp"
#include "../ConstantGradeDragState.hpp"

class ConstantGradeOperation;
class ConstantGradeSettings;

class ConstantGradeExecutingState final : public IControlState {
public:
    ConstantGradeExecutingState(
        ConstantGradeSettings& settings,
        ConstantGradeOperation& operation,
        ConstantGradeDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Executing; }
    const char* GetName() const override { return "ConstantGradeExecuting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;

private:
    ConstantGradeSettings& settings_;
    ConstantGradeOperation& operation_;
    ConstantGradeDragState& dragState_;
};
