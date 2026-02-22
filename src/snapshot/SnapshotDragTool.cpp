#include "SnapshotDragTool.hpp"

#include "cISC4City.h"
#include "cISC4View3DWin.h"
#include "cIGZWin.h"
#include "cIGZWinMgr.h"
#include "controls/InactiveState.hpp"
#include "controls/StatefulDragViewInputControl.hpp"
#include "snapshot/SnapshotManager.hpp"
#include "snapshot/states/SnapshotHoveringState.hpp"
#include "snapshot/states/SnapshotSelectingState.hpp"
#include "snapshot/states/SnapshotExecutingState.hpp"
#include "viz/OverlayDrawManager.hpp"
#include "utils/Logger.h"

void SnapshotInputControlReleaser::operator()(StatefulDragViewInputControl* control) const noexcept {
	if (control) {
		control->Release();
	}
}

class SnapshotRestoreInputControl final : public StatefulDragViewInputControl {
public:
	SnapshotRestoreInputControl(
		cISTETerrain* terrain,
		cIGZWin* window,
		cISC4View3DWin* view3D,
		SnapshotManager& mgr,
		SnapshotPreviewRenderer& renderer,
		SnapshotDragState& dragState)
		: StatefulDragViewInputControl(kControlId, kCursorId, terrain, window, view3D)
	{
		RegisterState(std::make_unique<InactiveState>());
		RegisterState(std::make_unique<SnapshotHoveringState>(mgr, renderer, dragState));
		RegisterState(std::make_unique<SnapshotSelectingState>(mgr, renderer, dragState));
		RegisterState(std::make_unique<SnapshotExecutingState>(mgr, renderer, dragState));

		TransitionTo(ControlStateId::Inactive);
	}

	bool Init() override {
		return cSC4BaseViewInputControl::Init();
	}

	void Activate() override {
		TransitionTo(ControlStateId::Hovering);
	}

private:
	static constexpr uint32_t kControlId = 0x7E5A9B03;
	static constexpr uint32_t kCursorId = 0xD9B4FFAAu; // Reuse existing cursor
};

SnapshotDragTool::SnapshotDragTool(SnapshotManager& mgr, SnapshotPreviewRenderer& renderer)
	: mgr_(mgr)
	, renderer_(renderer)
{
}

SnapshotDragTool::~SnapshotDragTool() {
	Deactivate();
}

void SnapshotDragTool::Activate(
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* windowMgr,
	cIGZImGuiService*,
	OverlayDrawManager& drawMgr)
{
	ActivateDirect(city, view3d, windowMgr, drawMgr);
}

void SnapshotDragTool::ActivateDirect(
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* winMgr,
	OverlayDrawManager& drawMgr)
{
	if (!city || !view3d || !winMgr) {
		LOG_ERROR("SnapshotDragTool::Activate: missing required services");
		return;
	}

	// Deactivate previous control if any
	if (control_) {
		Deactivate();
	}

	cISTETerrain* terrain = city->GetTerrain();
	cIGZWin* window = winMgr->GetMainWindow();
	if (!terrain || !window) {
		LOG_ERROR("SnapshotDragTool::Activate: missing terrain/window");
		return;
	}

	auto* newControl = new SnapshotRestoreInputControl(
		terrain, window, view3d, mgr_, renderer_, dragState_
	);
	newControl->AddRef();
	control_.reset(newControl);

	control_->Init();
	view3d_ = view3d;
	drawMgr_ = &drawMgr;

	// Set deactivate callback BEFORE activating (SC4 may call Deactivate during SetCurrentViewInputControl)
	control_->SetDeactivateCallback([this]() {
		// Just clean up our state, don't call Deactivate() recursively
		LOG_DEBUG("SnapshotDragTool: deactivate callback fired");
		renderer_.ClearAll();
		view3d_ = nullptr;
	});

	view3d->SetCurrentViewInputControl(control_.get(), cISC4View3DWin::ViewInputControlStackOperation_None);
	control_->Activate();

	LOG_INFO("SnapshotDragTool: Activated for snapshot index {}", dragState_.restoreIndex);
}

void SnapshotDragTool::Deactivate() {
	if (!control_) return;

	// Clear the callback first to prevent recursion
	control_->SetDeactivateCallback(nullptr);

	control_->ClearSelections();
	control_->ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
	control_->Deactivate();

	if (view3d_) {
		cISC4ViewInputControl* currentControl = view3d_->GetCurrentViewInputControl();
		if (currentControl == control_.get()) {
			view3d_->RemoveCurrentViewInputControl(false);
		}
	}

	// Don't unregister renderer from draw manager - it's permanently registered
	renderer_.ClearAll();

	control_->Shutdown();
	control_.reset();
	view3d_ = nullptr;
}

StatefulDragViewInputControl* SnapshotDragTool::GetInputControl() {
	return control_.get();
}
