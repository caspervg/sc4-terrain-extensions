#include "FlattenPickingHeightState.hpp"

#include <sstream>

#include "cISTETerrain.h"
#include "cRZBaseString.h"
#include "controls/StatefulDragViewInputControl.hpp"
#include "utils/Logger.h"

namespace {
constexpr int32_t kEscapeKey = 0x1B;

float SampleTileAverageHeight(cISTETerrain* terrain, const int32_t tileX, const int32_t tileZ) {
    return (
        terrain->GetAltitudeAtVertex(tileX, tileZ)
        + terrain->GetAltitudeAtVertex(tileX + 1, tileZ)
        + terrain->GetAltitudeAtVertex(tileX, tileZ + 1)
        + terrain->GetAltitudeAtVertex(tileX + 1, tileZ + 1)) / 4.0f;
}
}

FlattenPickingHeightState::FlattenPickingHeightState(
    FlattenRenderer& renderer,
    FlattenDragState& dragState)
    : renderer_(renderer)
    , dragState_(dragState) {
}

void FlattenPickingHeightState::OnEnter(StatefulDragViewInputControl& ctrl) {
    dragState_.ClearPickedReferenceTile();
    dragState_.selectionCommitted = false;
    ctrl.ClearSelections();
    renderer_.ClearAll();
    UpdateHintText_(ctrl, false, 0, 0);
}

void FlattenPickingHeightState::OnExit(StatefulDragViewInputControl& ctrl) {
    ctrl.ClearSelections();
    ctrl.ClearCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot);
    renderer_.ClearHoverTile();
}

bool FlattenPickingHeightState::OnMouseMove(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    uint32_t) {
    UpdateHover_(ctrl, x, z);
    return true;
}

bool FlattenPickingHeightState::OnMouseDownL(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z,
    uint32_t) {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        return false;
    }

    cISTETerrain* terrain = ctrl.GetTerrain();
    if (!terrain) {
        return false;
    }

    dragState_.pickedReferenceTileX = tileX;
    dragState_.pickedReferenceTileZ = tileZ;
    dragState_.pickedReferenceAverageHeight = SampleTileAverageHeight(terrain, tileX, tileZ);
    dragState_.hasPickedReferenceTile = true;
    dragState_.selectionCommitted = false;

    LOG_INFO(
        "FlattenInteractiveTool: picked reference tile ({}, {}) with average height {:.3f}m",
        tileX,
        tileZ,
        dragState_.pickedReferenceAverageHeight);

    ctrl.TransitionTo(ControlStateId::Hovering);
    return true;
}

bool FlattenPickingHeightState::OnMouseDownR(
    StatefulDragViewInputControl& ctrl,
    int32_t,
    int32_t,
    uint32_t) {
    dragState_.ClearPickedReferenceTile();
    ctrl.TransitionTo(ControlStateId::Hovering);
    return false;
}

bool FlattenPickingHeightState::OnKeyDown(
    StatefulDragViewInputControl& ctrl,
    const int32_t vk,
    uint32_t) {
    if (vk == kEscapeKey) {
        dragState_.ClearPickedReferenceTile();
        ctrl.TransitionTo(ControlStateId::Hovering);
        return true;
    }

    return false;
}

void FlattenPickingHeightState::UpdateHover_(
    StatefulDragViewInputControl& ctrl,
    const int32_t x,
    const int32_t z) const {
    int32_t tileX = 0;
    int32_t tileZ = 0;
    if (!ctrl.ScreenToTile(x, z, tileX, tileZ)) {
        ctrl.ClearSelections();
        renderer_.ClearHoverTile();
        UpdateHintText_(ctrl, false, 0, 0);
        return;
    }

    (void)ctrl.MarkSelected(
        tileX,
        tileZ,
        tileX,
        tileZ,
        cISTETerrain::eHilightColorType::Yellow,
        true);
    renderer_.ShowReferenceHoverTile(ctrl.GetTerrain(), tileX, tileZ);
    UpdateHintText_(ctrl, true, tileX, tileZ);
}

void FlattenPickingHeightState::UpdateHintText_(
    StatefulDragViewInputControl& ctrl,
    const bool hasHoverTile,
    const int32_t tileX,
    const int32_t tileZ) const {
    std::ostringstream body;
    body << "Click a reference tile";

    cISTETerrain* terrain = ctrl.GetTerrain();
    if (hasHoverTile && terrain) {
        body << "\naverage height " << SampleTileAverageHeight(terrain, tileX, tileZ) << "m";
    }

    body << "\nRight-click/Esc: cancel";

    const cRZBaseString title("Leveler [pick reference avg]");
    const cRZBaseString text(body.str().c_str());
    ctrl.SetCursorText(StatefulDragViewInputControl::kPrimaryCursorSlot, title, text);
}
