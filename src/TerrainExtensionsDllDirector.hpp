// src/TerrainExtensionsDllDirector.hpp
#pragma once

#include "cRZMessage2COMDirector.h"
#include "tools/TerrainToolRegistry.hpp"
#include "core/DragToolManager.hpp"
#include "viz/OverlayDrawManager.hpp"
#include "viz/TerrainContourRenderer.hpp"
#include "snapshot/SnapshotManager.hpp"
#include "snapshot/SnapshotPreviewRenderer.hpp"

#include <cstdint>
#include <string>

enum class DrawServicePass : uint8_t;
class cIGZMessageServer2;
class cIGZMessage2Standard;
class cIGZCheatCodeManager;
class cIGZImGuiService;
class cIGZS3DCameraService;
class cIGZDrawService;
class cISC4City;
class cISC4View3DWin;
class cIGZWinMgr;
class SnapshotPanel;
class SnapshotDragTool;
struct IDirect3DDevice7;

class TerrainExtensionsDllDirector final : public cRZMessage2COMDirector {
public:
    TerrainExtensionsDllDirector();
    ~TerrainExtensionsDllDirector() override;

    [[nodiscard]] uint32_t GetDirectorID() const override;
    bool     OnStart(cIGZCOM* pCOM) override;
    bool     DoMessage(cIGZMessage2* pMsg) override;
    bool     PostAppInit() override;

private:
    void PostCityInit_    (const cIGZMessage2Standard* pMsg);
    void PreCityShutdown_ (cIGZMessage2Standard* pMsg);
    void ProcessCheat_    (cIGZMessage2Standard* pMsg);

    void SetUpCommandTools_(cISC4City* pCity, cISTETerrain* pTerrain);
    void SetUpDragTools_   (cISC4City* pCity, cISC4View3DWin* pView3D);

    static void DrawOverlayCallback_(DrawServicePass pass, bool begin, void* pThis);

    void ShowMessageBox_(const std::string& title, const std::string& message) const;
    std::vector<std::string> SplitString_(const std::string& input);

    cIGZCheatCodeManager*  cheatCodeManager_;
    cISC4View3DWin*        view3d_;
    cISC4City*             city_;
    cIGZWinMgr*            winMgr_;
    cIGZMessageServer2*    messageServer_;
    cIGZImGuiService*      imguiService_;
    cIGZS3DCameraService*  cameraService_;
    cIGZDrawService*       drawService_;
    uint32_t               drawCallbackToken_;

    TerrainToolRegistry toolRegistry_;
    DragToolManager     dragToolManager_;
    OverlayDrawManager  overlayDrawManager_;
    TerrainContourRenderer contourRenderer_;

    SnapshotManager        snapshotManager_;
    SnapshotPreviewRenderer snapshotRenderer_;
    std::unique_ptr<SnapshotPanel> snapshotPanel_;
    SnapshotDragTool*      snapshotDragTool_{nullptr}; // Owned by dragToolManager_
    bool                   snapshotPanelRegistered_{false};
    bool                   snapshotPanelVisible_{false};
};
