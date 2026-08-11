#include "ConstantGradeExecutingState.hpp"

#include "controls/StatefulDragViewInputControl.hpp"
#include "../ConstantGradeOperation.hpp"
#include "../ConstantGradeRequestBuilder.hpp"
#include "../ConstantGradeSettings.hpp"
#include "utils/Logger.h"

ConstantGradeExecutingState::ConstantGradeExecutingState(
    ConstantGradeSettings& settings,
    ConstantGradeOperation& operation,
    ConstantGradeDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , dragState_(dragState) {
}

void ConstantGradeExecutingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    const auto request = BuildConstantGradeRequest(dragState_, settings_);
    if (!request.has_value()) {
        ctrl.TransitionTo(ControlStateId::Hovering);
        return;
    }

    ctrl.FireBeforeExecute();
    if (!operation_.Apply(*request)) {
        LOG_WARN("ConstantGradeExecutingState: grade apply failed");
    }

    ctrl.TransitionTo(ControlStateId::Hovering);
}
