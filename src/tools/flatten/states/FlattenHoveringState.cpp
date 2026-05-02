#include "FlattenHoveringState.hpp"

#include <sstream>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/ToolParameter.hpp"

namespace {
constexpr int32_t kTabKey = 0x09;
}

FlattenHoveringState::FlattenHoveringState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenRenderer& renderer,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void FlattenHoveringState::OnEnter(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    renderer_.ClearAll();
    UpdateHintText_(ctrl, 0);
}

void FlattenHoveringState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
    renderer_.ClearHoverTile();
}

bool FlattenHoveringState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    UpdateHoverSelection_(ctrl, x, z);
    UpdateHintText_(ctrl, mod);
    return true;
}

bool FlattenHoveringState::OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        return false;
    }

    dragState_.startX = tileX;
    dragState_.startZ = tileZ;
    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    ctrl.TransitionTo(ControlStateId::Selecting);
    return true;
}

bool FlattenHoveringState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t mod,
    int32_t delta) {
    return HandleAdjustment_(ctrl, mod, delta);
}

bool FlattenHoveringState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.Close();
        return true;
    }

    if (vk == kTabKey) {
        const int32_t delta = (mod & ModifierCombo::kShift) != 0 ? -1 : 1;
        if ((mod & ModifierCombo::kCtrl) != 0) {
            settings_.CycleShape(delta);
        } else {
            settings_.CycleMode(delta);
        }
        UpdateHintText_(ctrl, mod);
        return true;
    }

    UpdateHintText_(ctrl, mod);
    return false;
}

void FlattenHoveringState::UpdateHintText_(StatefulDragViewInputControl& ctrl, const uint32_t modifiers) const {
    std::ostringstream body;
    body << "Drag to flatten\n";
    body << settings_.ModeLabel() << " | " << settings_.ValueLabel();
    body << " | " << settings_.ShapeLabel();
    if (settings_.shape == FlattenShapeMode::LineMask) {
        body << ' ' << settings_.ThicknessLabel();
    }
    body << "\nTab/Shift+Tab: cycle mode";
    body << "\nCtrl+Tab/Ctrl+Shift+Tab: cycle shape";
    if (settings_.mode == FlattenHeightMode::Explicit || settings_.mode == FlattenHeightMode::Delta) {
        body << "\nAlt+Scroll: value (" << settings_.ValueLabel() << ")";
    }
    if (settings_.shape == FlattenShapeMode::LineMask) {
        body << "\nShift+Scroll: thickness (" << settings_.ThicknessLabel() << ")";
    }

    const cRZBaseString title("Flatten terrain");
    const cRZBaseString text(body.str().c_str());
    ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, text);
}

void FlattenHoveringState::UpdateHoverSelection_(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z) const {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        ctrl.ClearSelections();
        renderer_.ClearHoverTile();
        return;
    }

    (void)ctrl.MarkSelected(
        tileX,
        tileZ,
        tileX,
        tileZ,
        cISTETerrain::eHilightColorType::Blue,
        true);
    renderer_.ShowHoverTile(ctrl.GetTerrain(), tileX, tileZ);
}

bool FlattenHoveringState::HandleAdjustment_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers,
    const int32_t delta) const {
    const auto parameter = settings_.parameters.FindByModifiers(modifiers);
    if (!parameter.has_value()) {
        return false;
    }

    parameter->AdjustByDelta(delta);
    UpdateHintText_(ctrl, modifiers);
    return true;
}
