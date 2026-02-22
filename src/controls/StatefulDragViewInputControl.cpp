#include "StatefulDragViewInputControl.hpp"
#include "SC4Rect.h"
#include "cRZBaseString.h"

#include <algorithm>
#include <cassert>

#include "cISTETerrainView.h"
#include "utils/Logger.h"

StatefulDragViewInputControl::StatefulDragViewInputControl(
	const uint32_t controlId,
	const uint32_t cursorId,
	cISTETerrain* terrain,
	cIGZWin* window,
	cISC4View3DWin* view3D)
	  : cSC4BaseViewInputControl(controlId)
	  , terrain_(terrain, cRZAutoRefCount<cISTETerrain>::kAddRef)
	  , window_(window, cRZAutoRefCount<cIGZWin>::kAddRef)
	  , view3D_(view3D, cRZAutoRefCount<cISC4View3DWin>::kAddRef) {
	this->cursorID = cursorId;
}

void StatefulDragViewInputControl::RegisterState(std::unique_ptr<IControlState> state) {
	const ControlStateId id = state->GetStateId();
	assert(!states_.contains(id) && "Duplicate state registered");
	LOG_DEBUG("StatefulDragViewInputControl: registered state '{}'", state->GetName());
	states_.emplace(id, std::move(state));
}

void StatefulDragViewInputControl::TransitionTo(ControlStateId newId) {
	if (!states_.contains(newId)) {
		LOG_ERROR("StatefulDragViewInputControl: unknown state id '{}'", static_cast<uint32_t>(newId));
		assert(false && "Transition to unknown state");
		return;
	}

	const auto state = states_.at(newId).get();

	if (currentState_) {
		LOG_DEBUG("StatefulDragViewInputControl: transitioning '{}' -> '{}'", currentState_->GetName(),
		          state->GetName());
		currentState_->OnExit(*this);
	}
	else {
		LOG_DEBUG("StatefulDragViewInputControl: transitioning (null) -> '{}'", state->GetName());
	}

	currentState_ = state;
	currentState_->OnEnter(*this);
}

ControlStateId StatefulDragViewInputControl::GetCurrentStateId() const {
	if (!currentState_) return ControlStateId::Inactive;
	return currentState_->GetStateId();
}

IControlState* StatefulDragViewInputControl::GetCurrentState() const {
	return currentState_;
}

bool StatefulDragViewInputControl::ScreenToTile(int32_t screenX, int32_t screenZ, int32_t& outTileX,
                                                int32_t& outTileZ) const {
	if (!view3D_ || !terrain_) return false;

	float worldCoords[3] = {0.0f, 0.0f, 0.0f};
	const bool terrainQueryState = view3D_->GetTerrainQueryEnabled();
	const bool pickResult = view3D_->PickTerrain(
		screenX, screenZ, worldCoords, terrainQueryState);

	if (!pickResult) return false;

	const uint32_t maxX = terrain_->CellCountX() - 1;
	const uint32_t maxZ = terrain_->CellCountZ() - 1;

	outTileX = static_cast<int32_t>(
		std::clamp(
			static_cast<uint32_t>(worldCoords[0] / 16.0f),
			0u, maxX
		)
	);
	outTileZ = static_cast<int32_t>(
		std::clamp(
			static_cast<uint32_t>(worldCoords[2] / 16.0f),
			0u, maxZ
		)
	);

	return true;
}

bool StatefulDragViewInputControl::MarkSelected(
	const int32_t x1,
	const int32_t z1,
	const int32_t x2,
	const int32_t z2,
	const cISTETerrain::eHilightColorType color,
	const bool clearOthers) const
{
	if (!terrain_ || !terrain_->GetView()) return false;

	SC4Rect rect = {std::min(x1, x2), std::min(z1, z2), std::max(x1, x2), std::max(z1, z2)};

	const int maxX = static_cast<int>(terrain_->CellCountX()) - 1;
	const int maxZ = static_cast<int>(terrain_->CellCountZ()) - 1;

	if (rect.topLeftX < 0 || rect.topLeftY < 0 ||
		rect.bottomRightX > maxX || rect.bottomRightY > maxZ) {
		LOG_WARN("StatefulDragControl::MarkSelected: "
		         "rect ({},{}) -> ({},{}) out of bounds (max {}, {})",
		         rect.topLeftX, rect.topLeftY,
		         rect.bottomRightX, rect.bottomRightY,
		         maxX, maxZ);
		return false;
	}

	return terrain_->GetView()->MarkSelected(rect, color, clearOthers);
}

void StatefulDragViewInputControl::ClearSelections() const {
	if (terrain_ && terrain_->GetView()) {
		terrain_->GetView()->ClearCurrentSelections();
	}
}

void StatefulDragViewInputControl::SetCursorText(
	const uint32_t slot,
	const cRZBaseString& title,
	const cRZBaseString& body) const {
	if (view3D_) {
		view3D_->SetCursorText(slot, kPrimaryCursorPrio, &title, &body, 0);
	}
}

void StatefulDragViewInputControl::ClearCursorText(const uint32_t slot) const {
	if (view3D_) {
		view3D_->ClearCursorText(slot);
	}
}

bool StatefulDragViewInputControl::OnMouseMove(const int32_t x, const int32_t z, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseMove(*this, x, z, mod);
}

bool StatefulDragViewInputControl::OnMouseDownL(const int32_t x, const int32_t z, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseDownL(*this, x, z, mod);
}

bool StatefulDragViewInputControl::OnMouseUpL(const int32_t x, const int32_t z, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseUpL(*this, x, z, mod);
}

bool StatefulDragViewInputControl::OnMouseDownR(const int32_t x, const int32_t z, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseDownR(*this, x, z, mod);
}

bool StatefulDragViewInputControl::OnMouseUpR(const int32_t x, const int32_t z, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseUpR(*this, x, z, mod);
}

bool StatefulDragViewInputControl::OnMouseWheel(const int32_t x, const int32_t z, const uint32_t mod, const int32_t delta) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnMouseWheel(*this, x, z, mod, delta);
}

bool StatefulDragViewInputControl::OnMouseExit() {
	if (!currentState_) return false;
	return currentState_->OnMouseExit(*this);
}

bool StatefulDragViewInputControl::OnCharacter(const char value) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnCharacter(*this, value);
}

bool StatefulDragViewInputControl::OnKeyDown(const int32_t vk, const uint32_t mod) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnKeyDown(*this, vk, mod);
}

bool StatefulDragViewInputControl::OnKeyUp(const int32_t vkCode, const uint32_t modifiers) {
	if (!IsOnTop() || !currentState_) return false;
	return currentState_->OnKeyUp(*this, vkCode, modifiers);
}

void StatefulDragViewInputControl::Activate() {
	cSC4BaseViewInputControl::Activate();
}

void StatefulDragViewInputControl::Deactivate() {
	cSC4BaseViewInputControl::Deactivate();
	TransitionTo(ControlStateId::Inactive);

	if (onDeactivate_) {
		onDeactivate_();
	}
}