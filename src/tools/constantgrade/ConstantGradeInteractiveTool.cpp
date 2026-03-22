#include "ConstantGradeInteractiveTool.hpp"

#include "cISC4City.h"
#include "cISC4View3DWin.h"
#include "cIGZWin.h"
#include "cIGZWinMgr.h"
#include "controls/InactiveState.hpp"
#include "controls/StatefulDragViewInputControl.hpp"
#include "states/ConstantGradeExecutingState.hpp"
#include "states/ConstantGradeHoveringState.hpp"
#include "states/ConstantGradeSelectingState.hpp"
#include "utils/Logger.h"
#include "viz/OverlayDrawManager.hpp"

namespace {
class ConstantGradeViewInputControl final : public StatefulDragViewInputControl {
public:
    ConstantGradeViewInputControl(
        cISTETerrain* terrain,
        cIGZWin* window,
        cISC4View3DWin* view3D,
        ConstantGradeSettings& settings,
        ConstantGradeOperation& operation,
        ConstantGradeRenderer& renderer)
        : StatefulDragViewInputControl(kControlId, kCursorId, terrain, window, view3D) {
        RegisterState(std::make_unique<InactiveState>());
        RegisterState(std::make_unique<ConstantGradeHoveringState>(settings, renderer, dragState_));
        RegisterState(std::make_unique<ConstantGradeSelectingState>(settings, operation, renderer, dragState_));
        RegisterState(std::make_unique<ConstantGradeExecutingState>(settings, operation, dragState_));
        TransitionTo(ControlStateId::Inactive);
    }

    bool Init() override {
        return cSC4BaseViewInputControl::Init();
    }

    void Activate() override {
        TransitionTo(ControlStateId::Hovering);
    }

private:
    static constexpr uint32_t kControlId = 0x2099E822u;
    static constexpr uint32_t kCursorId = 0xD9B4FFAAu;

    ConstantGradeDragState dragState_{};
};
}

ConstantGradeInteractiveTool::ConstantGradeInteractiveTool()
    : renderer_(std::make_unique<ConstantGradeRenderer>()) {
}

ConstantGradeInteractiveTool::~ConstantGradeInteractiveTool() {
    Deactivate();
}

void ConstantGradeInteractiveTool::Activate(
    cISC4City* city,
    cISC4View3DWin* view3d,
    cIGZWinMgr* windowMgr,
    cIGZImGuiService*,
    OverlayDrawManager& drawMgr) {
    if (!city || !view3d || !windowMgr) {
        LOG_ERROR("ConstantGradeInteractiveTool::Activate: missing city/view3d/windowMgr");
        return;
    }

    if (control_) {
        Deactivate();
    }

    cISTETerrain* terrain = city->GetTerrain();
    cIGZWin* window = windowMgr->GetMainWindow();
    if (!terrain || !window) {
        LOG_ERROR("ConstantGradeInteractiveTool::Activate: missing terrain/window");
        return;
    }

    operation_ = std::make_unique<ConstantGradeOperation>(terrain);

    auto* newControl = new ConstantGradeViewInputControl(
        terrain,
        window,
        view3d,
        settings_,
        *operation_,
        *renderer_);
    newControl->AddRef();
    control_.reset(newControl);

    control_->Init();
    view3d_ = view3d;
    drawMgr_ = &drawMgr;
    drawMgr_->Register(renderer_.get());
    control_->SetOwnerDeactivateCallback([this]() {
        if (renderer_) {
            renderer_->ClearAll();
        }
        drawMgr_ = nullptr;
        view3d_ = nullptr;
    });
    control_->Activate();

    view3d->SetCurrentViewInputControl(
        control_.get(),
        cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl);

    LOG_INFO("ConstantGradeInteractiveTool: activated");
}

void ConstantGradeInteractiveTool::Deactivate() {
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

    operation_.reset();
    view3d_ = nullptr;
}

StatefulDragViewInputControl* ConstantGradeInteractiveTool::GetInputControl() {
    return control_.get();
}
