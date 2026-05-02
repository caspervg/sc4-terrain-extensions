#pragma once
#include "IControlState.hpp"


class DormantState final : public IControlState {
public:
    ControlStateId GetStateId() const override { return ControlStateId::Dormant; }
    const char* GetName() const override { return "Dormant"; }

    void OnEnter(StatefulDragViewInputControl&) override;
    void OnExit(StatefulDragViewInputControl&) override;
};
