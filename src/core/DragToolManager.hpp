#pragma once
#include <memory>
#include <optional>
#include <vector>

#include "IDragTool.hpp"
#include "viz/OverlayDrawManager.hpp"


class cISC4City;
class cISC4View3DWin;

class DragToolManager {
public:
    void Register(std::unique_ptr<IDragTool> tool);

    bool TryActivate(uint32_t cheatId, cISC4City*, cISC4View3DWin*, cIGZWinMgr*, cIGZImGuiService*, OverlayDrawManager&);

    void DeactivateAll();
    void Clear();

    bool HasActiveTool() const noexcept { return activeToolIdx_ >= 0; }

private:
    void DeactivateCurrent_(cISC4View3DWin* view3d);
    std::optional<IDragTool*> GetActiveTool_() const;

private:
    std::vector<std::unique_ptr<IDragTool>> tools_{};
    int32_t activeToolIdx_{-1};
    cISC4View3DWin* view3d_{nullptr};
};
