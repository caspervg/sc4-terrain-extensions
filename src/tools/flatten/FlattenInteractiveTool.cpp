#include "FlattenInteractiveTool.hpp"

#include "cISC4City.h"
#include "cISC4View3DWin.h"
#include "cIGZWin.h"
#include "cIGZWinMgr.h"
#include "controls/InactiveState.hpp"
#include "controls/StatefulDragViewInputControl.hpp"
#include "states/FlattenExecutingState.hpp"
#include "states/FlattenHoveringState.hpp"
#include "states/FlattenSelectingState.hpp"
#include "utils/Logger.h"
#include "viz/OverlayDrawManager.hpp"

namespace {
class FlattenViewInputControl final : public StatefulDragViewInputControl {
public:
    FlattenViewInputControl(
        cISTETerrain* terrain,
        cIGZWin* window,
        cISC4View3DWin* view3D,
        FlattenSettings& settings,
        FlattenOperation& operation,
        FlattenRenderer& renderer)
        : StatefulDragViewInputControl(kControlId, kCursorId, terrain, window, view3D) {
        RegisterState(std::make_unique<InactiveState>());
        RegisterState(std::make_unique<FlattenHoveringState>(settings, operation, renderer, dragState_));
        RegisterState(std::make_unique<FlattenSelectingState>(settings, operation, renderer, dragState_));
        RegisterState(std::make_unique<FlattenExecutingState>(settings, operation, dragState_));
        TransitionTo(ControlStateId::Inactive);
    }

    bool Init() override {
        return cSC4BaseViewInputControl::Init();
    }

    void Activate() override {
        TransitionTo(ControlStateId::Hovering);
    }

private:
    static constexpr uint32_t kControlId = 0x2099E811u;
    static constexpr uint32_t kCursorId = 0xD9B4FFAAu;

    FlattenDragState dragState_{};
};
}

FlattenInteractiveTool::FlattenInteractiveTool()
    : renderer_(std::make_unique<FlattenRenderer>()) {
}

FlattenInteractiveTool::~FlattenInteractiveTool() {
    Deactivate();
}

void FlattenInteractiveTool::Activate(
    cISC4City* city,
    cISC4View3DWin* view3d,
    cIGZWinMgr* windowMgr,
    cIGZImGuiService*,
    OverlayDrawManager& drawMgr) {
    if (!city || !view3d || !windowMgr) {
        LOG_ERROR("FlattenInteractiveTool::Activate: missing city/view3d/windowMgr");
        return;
    }

    if (control_) {
        Deactivate();
    }

    cISTETerrain* terrain = city->GetTerrain();
    cIGZWin* window = windowMgr->GetMainWindow();
    if (!terrain || !window) {
        LOG_ERROR("FlattenInteractiveTool::Activate: missing terrain/window");
        return;
    }

    operation_ = std::make_unique<FlattenOperation>(terrain);

    auto* newControl = new FlattenViewInputControl(
        terrain,
        window,
        view3d,
        settings_,
        *operation_,
        *renderer_);
    newControl->AddRef();
    control_.reset(newControl);

    control_->Init();
    control_->Activate();

    view3d_ = view3d;
    drawMgr_ = &drawMgr;
    drawMgr_->Register(renderer_.get());

    view3d->SetCurrentViewInputControl(
        control_.get(),
        cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl);

    LOG_INFO("FlattenInteractiveTool: activated");
}

void FlattenInteractiveTool::Deactivate() {
    if (control_) {
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

    operation_.reset();
    view3d_ = nullptr;
}

StatefulDragViewInputControl* FlattenInteractiveTool::GetInputControl() {
    return control_.get();
}
