#include "FlattenSelectingState.hpp"

#include <sstream>

#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "tools/ToolParameter.hpp"
#include "utils/Logger.h"

namespace {
constexpr int32_t kTabKey = 0x09;
constexpr int32_t kToggleShapeKey = 0x44; // D
constexpr int32_t kPickHeightKey = 0x48; // H

bool CanPickReferenceTile(const FlattenSettings& settings) noexcept {
    return settings.mode == FlattenHeightMode::ReferenceTileAverage;
}

bool UsePickedReferenceTile(const FlattenSettings& settings, const FlattenDragState& dragState) noexcept {
    return CanPickReferenceTile(settings) && dragState.hasPickedReferenceTile;
}

bool IsAltOnly(const uint32_t modifiers) noexcept {
    return (modifiers & ModifierCombo::kAlt) != 0
        && (modifiers & ModifierCombo::kShift) == 0
        && (modifiers & ModifierCombo::kCtrl) == 0;
}

std::string BuildTitle(const FlattenSettings& settings) {
    std::ostringstream title;
    title << "Leveler [" << settings.ModeLabel();
    if (settings.shape == FlattenShapeMode::LineMask) {
        title << " (D)";
    }
    title << "]";
    return title.str();
}
}

FlattenSelectingState::FlattenSelectingState(
    FlattenSettings& settings,
    FlattenOperation& operation,
    FlattenRenderer& renderer,
    FlattenDragState& dragState)
    : settings_(settings)
    , operation_(operation)
    , renderer_(renderer)
    , dragState_(dragState) {
}

void FlattenSelectingState::OnEnter(StatefulDragViewInputControl& ctrl) {
    dragState_.selectionCommitted = false;
    if (!ctrl.BeginCapture()) {
        LOG_WARN("FlattenSelectingState: failed to acquire mouse capture; aborting selection");
        ctrl.TransitionTo(ControlStateId::Hovering);
        return;
    }
    RebuildPreview_(ctrl, 0);
}

void FlattenSelectingState::OnExit(StatefulDragViewInputControl& ctrl) {
    if (!dragState_.selectionCommitted) {
        dragState_.ClearPickedReferenceTile();
    }
    ctrl.EndCapture();
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
    renderer_.ClearAll();
}

bool FlattenSelectingState::OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        return true;
    }

    if (tileX == dragState_.currentX && tileZ == dragState_.currentZ) {
        return true;
    }

    dragState_.currentX = tileX;
    dragState_.currentZ = tileZ;
    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        dragState_.currentX = tileX;
        dragState_.currentZ = tileZ;
    }

    if (!operation_.BuildPreview(BuildRequest_()).has_value()) {
        ctrl.TransitionTo(ControlStateId::Hovering);
        return true;
    }

    dragState_.selectionCommitted = true;
    ctrl.TransitionTo(ControlStateId::Executing);
    return true;
}

bool FlattenSelectingState::OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t, int32_t, uint32_t) {
    ctrl.TransitionTo(ControlStateId::Hovering);
    return false;
}

bool FlattenSelectingState::OnMouseWheel(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t mod,
    int32_t delta) {
    if (!HandleAdjustment_(ctrl, mod, delta)) {
        return false;
    }

    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) {
    if (vk == 0x1B) {
        ctrl.Close();
        return true;
    }

    if (vk == kPickHeightKey && CanPickReferenceTile(settings_)) {
        dragState_.selectionCommitted = false;
        ctrl.TransitionTo(ControlStateId::ToolSpecific);
        return true;
    }

    if (vk == kTabKey) {
        const int32_t delta = (mod & ModifierCombo::kShift) != 0 ? -1 : 1;
        dragState_.ClearPickedReferenceTile();
        settings_.CycleMode(delta);
        return RebuildPreview_(ctrl, mod);
    }

    if (vk == kToggleShapeKey) {
        settings_.CycleShape(1);
        return RebuildPreview_(ctrl, mod);
    }

    return RebuildPreview_(ctrl, mod);
}

bool FlattenSelectingState::RebuildPreview_(StatefulDragViewInputControl& ctrl, const uint32_t modifiers) const {
    const auto preview = operation_.BuildPreview(BuildRequest_());
    if (!preview.has_value()) {
        renderer_.ClearAll();
        ctrl.ClearSelections();
        return false;
    }

    ctrl.ClearSelections();
    renderer_.Update(ctrl.GetTerrain(), *preview);
    renderer_.ShowHoverTile(ctrl.GetTerrain(), dragState_.currentX, dragState_.currentZ);
    UpdateCursor_(ctrl, *preview, modifiers);
    return true;
}

FlattenRequest FlattenSelectingState::BuildRequest_() const {
    const bool usePickedReferenceTile = UsePickedReferenceTile(settings_, dragState_);
    return FlattenRequest{
        .x1 = dragState_.startX,
        .z1 = dragState_.startZ,
        .x2 = dragState_.currentX,
        .z2 = dragState_.currentZ,
        .referenceTileX = usePickedReferenceTile ? dragState_.pickedReferenceTileX : dragState_.startX,
        .referenceTileZ = usePickedReferenceTile ? dragState_.pickedReferenceTileZ : dragState_.startZ,
        .mode = settings_.mode,
        .shape = settings_.shape,
        .explicitHeight = settings_.explicitHeight.value,
        .deltaHeight = settings_.deltaHeight.value,
        .lineThickness = settings_.lineThickness.value
    };
}

void FlattenSelectingState::UpdateCursor_(
    StatefulDragViewInputControl& ctrl,
    const FlattenPreview& preview,
    const uint32_t modifiers) const {
    std::ostringstream body;
    body << "Release to apply\n";
    if (UsePickedReferenceTile(settings_, dragState_)) {
        body << "picked reference | target " << preview.targetHeight << "m";
    } else if (settings_.mode == FlattenHeightMode::Delta) {
        body << settings_.ModeLabel() << " | " << settings_.ValueLabel();
    } else {
        body << settings_.ModeLabel() << " | target " << preview.targetHeight << "m";
    }
    body << " | " << settings_.ShapeLabel();
    if (settings_.shape == FlattenShapeMode::LineMask) {
        body << ' ' << settings_.ThicknessLabel();
    }
    if (CanPickReferenceTile(settings_)) {
        body << "\nH: "
            << (UsePickedReferenceTile(settings_, dragState_) ? "repick reference tile" : "pick reference tile");
    }
    body << "\nTab/Shift+Tab: cycle mode";
    body << "\nD: toggle rectangle/diagonal";
    if (settings_.mode == FlattenHeightMode::Explicit || settings_.mode == FlattenHeightMode::Delta) {
        body << "\nAlt+Scroll: value (" << settings_.ValueLabel() << ")";
    }
    if (settings_.shape == FlattenShapeMode::LineMask) {
        body << "\nShift+Scroll: thickness (" << settings_.ThicknessLabel() << ")";
    }

    const std::string title = BuildTitle(settings_);
    const cRZBaseString titleText(title.c_str());
    const cRZBaseString text(body.str().c_str());
    ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, titleText, text);
}

bool FlattenSelectingState::HandleAdjustment_(
    StatefulDragViewInputControl& ctrl,
    const uint32_t modifiers,
    const int32_t delta) const {
    if (IsAltOnly(modifiers)
        && settings_.mode != FlattenHeightMode::Explicit
        && settings_.mode != FlattenHeightMode::Delta) {
        return false;
    }

    const auto parameter = settings_.parameters.FindByModifiers(modifiers);
    if (!parameter.has_value()) {
        return false;
    }

    parameter->AdjustByDelta(delta);
    return true;
}
