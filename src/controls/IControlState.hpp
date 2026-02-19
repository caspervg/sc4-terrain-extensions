#pragma once
#include <cstdint>
#include "ControlStateId.hpp"

class StatefulDragViewInputControl;

class IControlState {
public:
    virtual ~IControlState() = default;
    virtual ControlStateId GetStateId() const = 0;
    virtual const char* GetName() const = 0;

    virtual void OnEnter(StatefulDragViewInputControl& ctrl) {}
    virtual void OnExit(StatefulDragViewInputControl& ctrl) {}

    virtual bool OnMouseMove(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) { return false; }
    virtual bool OnMouseDownL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) { return false; }
    virtual bool OnMouseUpL(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) { return false; }
    virtual bool OnMouseDownR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) { return false; }
    virtual bool OnMouseUpR(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod) { return false; }
    virtual bool OnMouseWheel(StatefulDragViewInputControl& ctrl, int32_t x, int32_t z, uint32_t mod, int32_t delta) { return false; }
    virtual bool OnMouseExit(StatefulDragViewInputControl& ctrl) { return false; }

    virtual bool OnKeyDown(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) { return false; }
    virtual bool OnKeyUp(StatefulDragViewInputControl& ctrl, int32_t vk, uint32_t mod) { return false; }
    virtual bool OnCharacter(StatefulDragViewInputControl& ctrl, char value) { return false; }
};
