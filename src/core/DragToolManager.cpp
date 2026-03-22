#include "DragToolManager.hpp"

#include <utils/Logger.h>

#include "cISC4View3DWin.h"
#include "controls/StatefulDragViewInputControl.hpp"

void DragToolManager::Register(std::unique_ptr<IDragTool> tool) {
	LOG_DEBUG("DragToolManager: registering tool '{}'", tool->GetCheatName());
	tools_.push_back(std::move(tool));
}

bool DragToolManager::TryActivate(
	const uint32_t cheatId,
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* winManager,
	cIGZImGuiService* imguiService,
	OverlayDrawManager& overlayManager) {
	int32_t candidateIdx = -1;
	for (auto i = 0; i < tools_.size(); ++i) {
		if (tools_[i]->GetCheatId() == cheatId) {
			candidateIdx = i;
			break;
		}
	}

	if (candidateIdx < 0) return false;

	IDragTool* candidate = tools_[candidateIdx].get();

	if (activeToolIdx_ == candidateIdx) {
		DeactivateCurrent_(view3d);
		return true;
	}

	if (activeToolIdx_ >= 0) {
		DeactivateCurrent_(view3d);
	}

	candidate->Activate(city, view3d, winManager, imguiService, overlayManager);
	if (auto* control = candidate->GetInputControl()) {
		activeToolIdx_ = candidateIdx;
		view3d_ = view3d;
		control->SetDeactivateCallback([this]() {
			activeToolIdx_ = -1;
			view3d_ = nullptr;
		});
	} else {
		LOG_WARN("DragToolManager: activation of '{}' did not produce an input control", candidate->GetCheatName());
		activeToolIdx_ = -1;
		view3d_ = nullptr;
		return false;
	}

	return true;
}

void DragToolManager::DeactivateAll() {
	if (activeToolIdx_ < 0) return;

	DeactivateCurrent_(view3d_);
}

void DragToolManager::Clear() {
	DeactivateAll();
	tools_.clear();
	activeToolIdx_ = -1;
	view3d_ = nullptr;
}

void DragToolManager::DeactivateCurrent_(cISC4View3DWin* view3d) {
	if (activeToolIdx_ < 0) return;

	IDragTool* active = tools_[activeToolIdx_].get();

	StatefulDragViewInputControl* control = active->GetInputControl();
	if (control && view3d) {
		cISC4ViewInputControl* currentControl = view3d->GetCurrentViewInputControl();
		if (currentControl == control) {
			view3d->SetCurrentViewInputControl(
				nullptr,
				cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl
			);
		}
	}

	active->Deactivate();
	activeToolIdx_ = -1;
}

std::optional<IDragTool*> DragToolManager::GetActiveTool_() const {
	if (activeToolIdx_ < 0) return std::nullopt;
	return tools_[activeToolIdx_].get();
}
