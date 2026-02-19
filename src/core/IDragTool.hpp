#pragma once
#include <cstdint>

#include "cIGZWin.h"

class cIGZImGuiService;
class StatefulDragViewInputControl;
class OverlayDrawManager;
class cISC4View3DWin;
class cISC4City;

class IDragTool {
public:
    virtual ~IDragTool() = default;
    virtual const char* GetCheatName() const = 0;
    virtual const uint32_t GetCheatId() const = 0;

    virtual void Activate(cISC4City* city, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&) = 0;
    virtual void Deactivate() = 0;

    virtual StatefulDragViewInputControl* GetInputControl() = 0;
};
