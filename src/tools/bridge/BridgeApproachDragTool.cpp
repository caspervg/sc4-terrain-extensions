#include "BridgeApproachDragTool.hpp"

#include <utils/Logger.h>

#include "cISC4City.h"
#include "BridgeDragState.hpp"
#include "controls/InactiveState.hpp"
#include "controls/StatefulDragViewInputControl.hpp"
#include "states/BridgeExecutingState.hpp"
#include "states/BridgeHoveringState.hpp"
#include "states/BridgeSelectingState.hpp"
#include "viz/OverlayDrawManager.hpp"

class BridgeDragViewInputControl final : public StatefulDragViewInputControl {
public:
	BridgeDragViewInputControl(
		cISTETerrain* terrain,
		cIGZWin* window,
		cISC4View3DWin* view3D,
		BridgeToolSettings& settings,
		BridgeApproachRenderer& renderer)
		: StatefulDragViewInputControl(kControlId, kCursorId, terrain, window, view3D)
		, settings_(settings)
		, renderer_(renderer)
	{
		RegisterState(std::make_unique<InactiveState>());
		RegisterState(std::make_unique<BridgeHoveringState>(settings, renderer, dragState_));
		RegisterState(std::make_unique<BridgeSelectingState>(settings, renderer, dragState_));
		RegisterState(std::make_unique<BridgeExecutingState>(settings, dragState_));

		TransitionTo(ControlStateId::Inactive);
	}

	bool Init() override {
		return cSC4BaseViewInputControl::Init();
	}

	void Activate() override {
		TransitionTo(ControlStateId::Hovering);
	}

	[[nodiscard]] BridgeToolSettings& GetSettings() const { return settings_; }
	[[nodiscard]] BridgeApproachRenderer& GetRenderer() const { return renderer_; }

private:
	static constexpr auto kControlId{0xA20FD559u};
	static constexpr auto kCursorId{0xD9B4FFAAu};

	BridgeToolSettings& settings_;
	BridgeApproachRenderer& renderer_;
	BridgeDragState dragState_{};
};

BridgeApproachDragTool::BridgeApproachDragTool()
	: renderer_(std::make_unique<BridgeApproachRenderer>())
{
}

BridgeApproachDragTool::~BridgeApproachDragTool() {
	Deactivate();
}

void BridgeApproachDragTool::Activate(
	cISC4City* city,
	cISC4View3DWin* view3d,
	cIGZWinMgr* windowMgr,
	cIGZImGuiService* imguiService,
	OverlayDrawManager& drawMgr)
{
	if (!city || !view3d || !windowMgr) {
		LOG_ERROR("BridgeApproachDragTool::Activate: missing city/view3d/windowMgr");
		return;
	}

	if (control_) {
		Deactivate();
	}

	cISTETerrain* terrain = city->GetTerrain();
	cIGZWin* window = windowMgr->GetMainWindow();
	if (!terrain || !window) {
		LOG_ERROR("BridgeApproachDragTool::Activate: missing terrain/window");
		return;
	}

	auto* newControl = new BridgeDragViewInputControl(
		terrain, window, view3d, settings_, *renderer_
	);
	newControl->AddRef();
	control_.reset(newControl);

	control_->Init();
	view3d_ = view3d;
	drawMgr_ = &drawMgr;
	drawMgr.Register(renderer_.get());
	control_->SetOwnerDeactivateCallback([this]() {
		if (renderer_) {
			renderer_->ClearAll();
		}
		view3d_ = nullptr;
	});
	control_->Activate();

	view3d->SetCurrentViewInputControl(
		control_.get(),
		cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl);

	LOG_INFO("BridgeApproachDragTool: Activated");
}

void BridgeApproachDragTool::Deactivate() {
	if (control_) {
		control_->SetOwnerDeactivateCallback(nullptr);
		control_->SetDeactivateCallback(nullptr);
		control_->ClearSelections();
		control_->ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
		control_->Deactivate();
	}

	if (control_ && view3d_) {
		cISC4ViewInputControl* currentControl = view3d_->GetCurrentViewInputControl();
		if (currentControl == control_.get()) {
			view3d_->RemoveCurrentViewInputControl(false);
		}
	}

	if (renderer_) {
		renderer_->ClearAll();
		if (drawMgr_) {
			drawMgr_->Unregister(renderer_.get());
			drawMgr_ = nullptr;
		}
	}

	if (control_) {
		control_->Shutdown();
	}
	control_.reset();

	view3d_ = nullptr;
}

StatefulDragViewInputControl* BridgeApproachDragTool::GetInputControl() {
	return control_.get();
}

const BridgeToolSettings& BridgeApproachDragTool::GetSettings() const {
	return settings_;
}
