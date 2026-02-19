#include "BridgeApproachDragTool.hpp"

#include <utils/Logger.h>

#include "cISC4City.h"
#include "IBridgeDragContext.hpp"
#include "controls/InactiveState.hpp"
#include "controls/StatefulDragViewInputControl.hpp"
#include "public/cIGZImGuiService.h"
#include "public/ImGuiPanelAdapter.h"
#include "states/BridgeExecutingState.hpp"
#include "states/BridgeHoveringState.hpp"
#include "states/BridgeSelectingState.hpp"
#include "viz/OverlayDrawManager.hpp"

void ViewInputControlReleaser::operator()(StatefulDragViewInputControl* control) const noexcept {
	if (control) {
		control->Release();
	}
}

class BridgeDragViewInputControl final : public StatefulDragViewInputControl, public IBridgeDragContext {
public:
	BridgeDragViewInputControl(
		cISTETerrain* terrain,
		cIGZWin* window,
		cISC4View3DWin* view3D,
		BridgeToolSettings& settings,
		BridgeApproachRenderer& renderer)
		: StatefulDragViewInputControl(kControlId, kCursorId, terrain, window, view3D)
		, IBridgeDragContext()
		, settings_(settings)
		, renderer_(renderer)
	{
		RegisterState(std::make_unique<InactiveState>());
		RegisterState(std::make_unique<BridgeHoveringState>(settings, renderer, *this));
		RegisterState(std::make_unique<BridgeSelectingState>(settings, renderer, *this));
		RegisterState(std::make_unique<BridgeExecutingState>(settings, *this));

		TransitionTo(ControlStateId::Inactive);
	}

	bool Init() override {
		return cSC4BaseViewInputControl::Init();
	}

	void Activate() override {
		TransitionTo(ControlStateId::Hovering);
	}

	BridgeToolSettings& GetSettings() const { return settings_; }
	BridgeApproachRenderer& GetRenderer() const { return renderer_; }

	int32_t GetDragStartX()   const noexcept override { return dragStartX_; }
	int32_t GetDragStartZ()   const noexcept override { return dragStartZ_; }
	int32_t GetDragCurrentX() const noexcept override { return dragCurrentX_; }
	int32_t GetDragCurrentZ() const noexcept override { return dragCurrentZ_; }

	void SetDragStart(const int32_t x, const int32_t z) noexcept override {
		dragStartX_ = x;
		dragStartZ_ = z;
	}
	void SetDragCurrent(const int32_t x, const int32_t z) noexcept override {
		dragCurrentX_ = x;
		dragCurrentZ_ = z;
	}

	std::optional<BridgePlacement> ComputePlacement() const override {
		return ComputeBridgePlacement(
			dragStartX_, dragStartZ_,
			dragCurrentX_, dragCurrentZ_
		);
	}

private:
	static constexpr auto kControlId{0xA20FD559u};
	static constexpr auto kCursorId{0xD9B4FFAAu};

	BridgeToolSettings& settings_;
	BridgeApproachRenderer& renderer_;

	int32_t dragStartX_{0};
	int32_t dragStartZ_{0};
	int32_t dragCurrentX_{0};
	int32_t dragCurrentZ_{0};
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
	control_->Activate();
	view3d_ = view3d;

	drawMgr_ = &drawMgr;
	drawMgr.Register(renderer_.get());

	if (imguiService) {
		imguiService_ = imguiService;

		panel_ = std::make_unique<BridgeToolPanel>(settings_);
		const ImGuiPanelDesc desc = ImGuiPanelAdapter<BridgeToolPanel>::MakeDesc(
			panel_.get(), kBridgeToolPanelId, 100, false
		);
		if (imguiService_->RegisterPanel(desc)) {
			panelRegistered_ = true;
			panel_->SetOpen(true);
			LOG_INFO("BridgeApproachDragTool: ImGui panel registered");
		}
	}

	view3d->SetCurrentViewInputControl(control_.get(), cISC4View3DWin::ViewInputControlStackOperation_None);

	LOG_INFO("BridgeApproachDragTool: Activated");
}

void BridgeApproachDragTool::Deactivate() {
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

	if (panelRegistered_ && panel_) {
		if (imguiService_) {
			imguiService_->UnregisterPanel(kBridgeToolPanelId);
		}
		panelRegistered_ = false;
	}

	panel_.reset();
	if (control_) {
		control_->Shutdown();
	}
	control_.reset();
	view3d_ = nullptr;

	LOG_INFO("BridgeApproachDragTool: Deactivated");
}

StatefulDragViewInputControl* BridgeApproachDragTool::GetInputControl() {
	return control_.get();
}

const BridgeToolSettings& BridgeApproachDragTool::GetSettings() const {
	return settings_;
}
