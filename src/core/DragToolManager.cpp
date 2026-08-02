#include "DragToolManager.hpp"

#include <utils/Logger.h>

#include "cISC4View3DWin.h"
#include "controls/StatefulDragViewInputControl.hpp"

void DragToolManager::Register(std::unique_ptr<IDragTool> tool) {
	LOG_DEBUG("DragToolManager: registering tool '{}'", tool->GetCheatName());
	tools_.push_back(std::move(tool));
}

int32_t DragToolManager::FindToolIndex_(const uint32_t cheatId) const {
	for (auto i = 0; i < tools_.size(); ++i) {
		if (tools_[i]->GetCheatId() == cheatId) {
			return static_cast<int32_t>(i);
		}
	}
	return -1;
}

bool DragToolManager::ActivateIndex_(
	const int32_t idx,
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* winManager,
	cIGZImGuiService* imguiService,
	OverlayDrawManager& overlayManager,
	SnapshotManager* snapshotManager) {
	if (activeToolIdx_ >= 0) {
		DeactivateCurrent_(view3d);
	}

	IDragTool* candidate = tools_[idx].get();
	candidate->Activate(city, view3d, winManager, imguiService, overlayManager, snapshotManager);
	if (auto* control = candidate->GetInputControl()) {
		activeToolIdx_ = idx;
		view3d_ = view3d;
		control->SetDeactivateCallback([this]() {
			activeToolIdx_ = -1;
			view3d_ = nullptr;
		});
		return true;
	}

	LOG_WARN("DragToolManager: activation of '{}' did not produce an input control", candidate->GetCheatName());
	activeToolIdx_ = -1;
	view3d_ = nullptr;
	return false;
}

bool DragToolManager::TryActivate(
	const uint32_t cheatId,
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* winManager,
	cIGZImGuiService* imguiService,
	OverlayDrawManager& overlayManager,
	SnapshotManager* snapshotManager) {
	const int32_t candidateIdx = FindToolIndex_(cheatId);
	if (candidateIdx < 0) return false;

	if (activeToolIdx_ == candidateIdx) {
		DeactivateCurrent_(view3d);
		return true;
	}

	return ActivateIndex_(candidateIdx, city, view3d, winManager, imguiService, overlayManager, snapshotManager);
}

bool DragToolManager::ActivateTool(
	const uint32_t cheatId,
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* winManager,
	cIGZImGuiService* imguiService,
	OverlayDrawManager& overlayManager,
	SnapshotManager* snapshotManager) {
	const int32_t candidateIdx = FindToolIndex_(cheatId);
	if (candidateIdx < 0) return false;

	return ActivateIndex_(candidateIdx, city, view3d, winManager, imguiService, overlayManager, snapshotManager);
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
