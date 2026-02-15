#pragma once
#include <cstdint>
#include <string_view>
#include "cRZMessage2COMDirector.h"
#include "cRZAutoRefCount.h"
#include "tools/TerrainToolRegistry.hpp"
#include "controls/BaseDragViewInputControl.cpp"
#include "public/cIGZDrawService.h"

class cIGZMessage2Standard;
class cIGZMessageServer2;
class cIGZWinMgr;
class cISC4City;
class cISC4View3DWin;
class cIGZCheatCodeManager;
class cIGZMessage2;
class BridgeToolPanel;
class cIGZImGuiService;
class cIGZS3DCameraService;

static constexpr uint32_t kMessageCheatIssued = 0x230E27AC;
static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

static constexpr uint32_t kTerrainExtensionsDirectorID = 0xA20FD558;
// Randomly generated ID to avoid conflicts with other mods

static constexpr uint32_t kTerrainExtensionsCheatID = 0x903d4018;
// Randomly generated ID for the cheat code to avoid conflicts with other mods
static constexpr std::string_view kTerrainExtensionsCheatString = "earthbender";

static constexpr uint32_t kTerrainExtensionsBridgeCheatID = 0x9773F4CD;
static constexpr std::string_view kTerrainExtensionsBridgeCheatString = "bridgebuilder";

static cIGZImGuiService* gImGuiService;

class TerrainExtensionsDllDirector final : public cRZMessage2COMDirector {
public:
    TerrainExtensionsDllDirector();
    ~TerrainExtensionsDllDirector() override;

    [[nodiscard]] uint32_t GetDirectorID() const override;
    bool OnStart(cIGZCOM* pCOM) override;

    bool PostAppInit() override;
    bool DoMessage(cIGZMessage2* pMsg) override;

private:
    void PostCityInit_(const cIGZMessage2Standard* pStandardMsg);
    void PreCityShutdown_(cIGZMessage2Standard* pStandardMsg);
    void SetUpTools_(cISC4City* pCity, cISTETerrain* pTerrain);
    void ProcessCheat_(cIGZMessage2Standard* pStandardMsg);
    void ActivateBridgeDragMode_();
    bool ActivateDragControl_(BaseDragViewInputControl* control);
    void ShowMessageBox_(const std::string& title, const std::string& message) const;
    static void DrawBridgeVisualizerCallback_(DrawServicePass pass, bool begin, void*);


private:
    cIGZCheatCodeManager* pCheatCodeManager;
    cISC4View3DWin* pView3D;
    cISC4City* pCity;
    cIGZWinMgr* pWinMgr;
    TerrainToolRegistry mToolRegistry;
    cRZAutoRefCount<BaseDragViewInputControl> mActiveDragControl;
    cIGZMessageServer2* pMS2;
    cIGZImGuiService* pImGui;
    cIGZS3DCameraService* pCamera;
    cIGZDrawService* pDraw;
    uint32_t nBridgeDrawCallbackToken;
    std::unique_ptr<BridgeToolPanel> pPanel;
    bool bPanelRegistered;
    bool bPanelVisible;
};