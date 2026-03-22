#pragma once

#include "controls/IControlState.hpp"

#include "../FlattenDragState.hpp"
#include "../FlattenOperation.hpp"
#include "../FlattenSettings.hpp"

class FlattenExecutingState final : public IControlState {
public:
    FlattenExecutingState(FlattenSettings& settings, FlattenOperation& operation, FlattenDragState& dragState);

    ControlStateId GetStateId() const override { return ControlStateId::Executing; }
    const char* GetName() const override { return "FlattenExecuting"; }

    void OnEnter(StatefulDragViewInputControl& ctrl) override;

private:
    [[nodiscard]] FlattenRequest BuildRequest_() const;

private:
    FlattenSettings& settings_;
    FlattenOperation& operation_;
    FlattenDragState& dragState_;
};
