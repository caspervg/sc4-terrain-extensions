#include "InactiveState.hpp"

#include "StatefulDragViewInputControl.hpp"
#include "utils/Logger.h"

void InactiveState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryTextSlot);
	ctrl.ClearCursorText(StatefulDragViewInputControl::kSecondaryTextSlot);
	ctrl.ClearSelections();

	LOG_DEBUG("InactiveState::OnEnter - control deactivated");
}

void InactiveState::OnExit(StatefulDragViewInputControl&) {
	LOG_DEBUG("InactivateState::OnExit - control activating");
}