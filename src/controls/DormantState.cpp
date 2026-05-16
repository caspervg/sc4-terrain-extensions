#include "DormantState.hpp"

#include "StatefulDragViewInputControl.hpp"
#include "utils/Logger.h"

void DormantState::OnEnter(StatefulDragViewInputControl& ctrl) {
	ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
	ctrl.ClearSelections();

	LOG_DEBUG("DormantState::OnEnter - control parked");
}

void DormantState::OnExit(StatefulDragViewInputControl&) {
	LOG_DEBUG("DormantState::OnExit - control activating");
}
