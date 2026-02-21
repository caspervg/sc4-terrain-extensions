#pragma once
#include <memory>
#include <unordered_map>

#include "cISTETerrain.h"
#include "cSC4BaseViewInputControl.h"
#include "IControlState.hpp"


class cRZBaseString;

class StatefulDragViewInputControl : public cSC4BaseViewInputControl {
public:
    StatefulDragViewInputControl(uint32_t controlId, uint32_t cursorId, cISTETerrain* terrain, cIGZWin* window,
                                 cISC4View3DWin* view3D);

    void RegisterState(std::unique_ptr<IControlState> state);
    void TransitionTo(ControlStateId newId);

    ControlStateId GetCurrentStateId() const;
    IControlState* GetCurrentState() const;

    cISTETerrain* GetTerrain() const { return terrain_; }
    cIGZWin* GetWindow() const { return window_; }
    cISC4View3DWin* GetView3D() const { return view3D_; }

    bool ScreenToTile(int32_t screenX, int32_t screenZ, int32_t& outTileX, int32_t& outTileZ) const;

    bool MarkSelected(int32_t x1, int32_t z1, int32_t x2, int32_t z2,
                      cISTETerrain::eHilightColorType color, bool clearOthers = true) const;
    void ClearSelections() const;

    void SetCursorText(uint32_t slot, const cRZBaseString& title, const cRZBaseString& body) const;
    void ClearCursorText(uint32_t slot) const;

    bool BeginCapture() { return SetCapture(); }
    bool EndCapture() { return ReleaseCapture(); }

    bool OnMouseMove(int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownL(int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpL(int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseDownR(int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseUpR(int32_t x, int32_t z, uint32_t mod) override;
    bool OnMouseWheel(int32_t x, int32_t z, uint32_t mod, int32_t delta) override;
    bool OnMouseExit() override;

    bool OnCharacter(char value) override;
    bool OnKeyDown(int32_t vk, uint32_t mod) override;
    bool OnKeyUp(int32_t vkCode, uint32_t modifiers) override;

public:
    static constexpr uint32_t kPrimaryCursorSlot = 0x234FE82Bu;
    static constexpr uint32_t kPrimaryCursorPrio = 0;

private:
    std::unordered_map<ControlStateId, std::unique_ptr<IControlState>> states_;
    IControlState* currentState_ = nullptr;

    cRZAutoRefCount<cISTETerrain> terrain_;
    cRZAutoRefCount<cIGZWin> window_;
    cRZAutoRefCount<cISC4View3DWin> view3D_;
};
