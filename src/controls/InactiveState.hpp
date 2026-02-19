#pragma once
#include "IControlState.hpp"


class InactiveState final : public IControlState {
public:
    ControlStateId GetStateId() const override { return ControlStateId::Inactive; }
    const char* GetName() const override { return "Inactive"; }

    void OnEnter(StatefulDragViewInputControl&) override;
    void OnExit(StatefulDragViewInputControl&) override;
};
