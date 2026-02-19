// src/TerrainExtensionsDllDirector.hpp
#pragma once

#include "cRZMessage2COMDirector.h"
#include "tools/TerrainToolRegistry.hpp"
#include "core/DragToolManager.hpp"
#include "viz/OverlayDrawManager.hpp"

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
struct IDirect3DDevice7;

class TerrainExtensionsDllDirector final : public cRZMessage2COMDirector {
public:
    TerrainExtensionsDllDirector();
    ~TerrainExtensionsDllDirector() override;

    uint32_t GetDirectorID() const override;
    bool     OnStart(cIGZCOM* pCOM) override;
    bool     DoMessage(cIGZMessage2* pMsg) override;
    bool     PostAppInit() override;

private:
    // ── Message handlers ──────────────────────────────────────────────────────
    void PostCityInit_    (const cIGZMessage2Standard* pMsg);
    void PreCityShutdown_ (cIGZMessage2Standard* pMsg);
    void ProcessCheat_    (cIGZMessage2Standard* pMsg);

    // ── Setup ─────────────────────────────────────────────────────────────────
    void SetUpCommandTools_(cISC4City* pCity, cISTETerrain* pTerrain);
    void SetUpDragTools_   (cISC4City* pCity, cISC4View3DWin* pView3D);

    // ── Draw callback ─────────────────────────────────────────────────────────
    static void DrawOverlayCallback_(DrawServicePass pass, bool begin, void* pThis);

    // ── Utilities ─────────────────────────────────────────────────────────────
    void ShowMessageBox_(const std::string& title, const std::string& message) const;
    std::vector<std::string> SplitString_(const std::string& input);

    // ── Service pointers — non-owning ─────────────────────────────────────────
    cIGZCheatCodeManager*  cheatCodeManager_;
    cISC4View3DWin*        view3d_;
    cISC4City*             city_;
    cIGZWinMgr*            winMgr_;
    cIGZMessageServer2*    messageServer_;
    cIGZImGuiService*      imguiService_;
    cIGZS3DCameraService*  cameraService_;
    cIGZDrawService*       drawService_;
    uint32_t               drawCallbackToken_;

    // ── Plugin subsystems ─────────────────────────────────────────────────────
    TerrainToolRegistry toolRegistry_;
    DragToolManager     dragToolManager_;
    OverlayDrawManager  overlayDrawManager_;
};