#include "TerrainExtensionsDllDirector.hpp"

#include "version.h"
#include "utils/Logger.h"
#include "cIGZApp.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZFrameWork.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cIGZWin.h"
#include "cIGZWinMgr.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4View3DWin.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"
#include "args.hxx"

#include "tools/ConstantGradeTool.cpp"
#include "tools/FlattenTool.cpp"
#include "tools/bridge/BridgeApproachCommand.hpp"
#include "tools/BlueprintCaptureTool.cpp"
#include "tools/BlueprintExportTool.cpp"
#include "tools/BlueprintStampTool.cpp"

#include "tools/bridge/BridgeApproachDragTool.hpp"

#include "public/cIGZImGuiService.h"
#include "public/ImGuiServiceIds.h"
#include "public/S3DCameraServiceIds.h"

#include <sstream>
#include <windows.h>

#include "public/cIGZDrawService.h"


static constexpr uint32_t kTerrainExtensionsDirectorID     = 0x2099E7AB; // your actual ID
static constexpr uint32_t kTerrainExtensionsCheatID        = 0x1234ABCD; // your actual ID
static constexpr uint32_t kTerrainExtensionsBridgeCheatID  = 0x9773F4CD; // your actual ID
static constexpr std::string_view kTerrainExtensionsCheatString = "earthbender";
static constexpr std::string_view kTerrainExtensionsBridgeCheatString = "bridgebuilder";
static constexpr uint32_t kSC4MessageCheatIssued = 0x230E27AC;
static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

TerrainExtensionsDllDirector::TerrainExtensionsDllDirector()
    : cheatCodeManager_(nullptr)
    , view3d_(nullptr)
    , city_(nullptr)
    , winMgr_(nullptr)
    , messageServer_(nullptr)
    , imguiService_(nullptr)
    , cameraService_(nullptr)
    , drawService_(nullptr)
    , drawCallbackToken_(0)
{
    Logger::Initialize("SC4TerrainExtensions");
    LOG_INFO("SC4TerrainExtensions v{}", PLUGIN_VERSION_STR);
}

TerrainExtensionsDllDirector::~TerrainExtensionsDllDirector() = default;

uint32_t TerrainExtensionsDllDirector::GetDirectorID() const {
    return kTerrainExtensionsDirectorID;
}

bool TerrainExtensionsDllDirector::OnStart(cIGZCOM* pCOM) {
    mpFrameWork->AddHook(this);
    return true;
}


bool TerrainExtensionsDllDirector::DoMessage(cIGZMessage2* pMsg) {
    const auto pStandardMsg = static_cast<cIGZMessage2Standard*>(pMsg);

    switch (pMsg->GetType()) {
    case kSC4MessageCheatIssued:
        ProcessCheat_(pStandardMsg);
        break;
    case kSC4MessagePostCityInit:
        PostCityInit_(pStandardMsg);
        break;
    case kSC4MessagePreCityShutdown:
        PreCityShutdown_(pStandardMsg);
        break;
    default:
        LOG_DEBUG("Unsupported message type: 0x{:X}", pMsg->GetType());
        break;
    }

    return true;
}


bool TerrainExtensionsDllDirector::PostAppInit() {
    cIGZMessageServer2Ptr pMS2;
    LOG_INFO("PostAppInit: Initializing TerrainExtensionsDllDirector");

    cIGZApp* const pApp = mpFrameWork->Application();
    if (pApp) {
        cRZAutoRefCount<cISC4App> pSC4App;
        if (pApp->QueryInterface(GZIID_cISC4App, pSC4App.AsPPVoid())) {
            cheatCodeManager_ = pSC4App->GetCheatCodeManager();
            winMgr_           = pSC4App->GetMainWindow()->GetWindowManager();
        }
    }

    if (pMS2) {
        pMS2->AddNotification(this, kSC4MessagePostCityInit);
        pMS2->AddNotification(this, kSC4MessagePreCityShutdown);
        messageServer_ = pMS2;
    }

    return true;
}


void TerrainExtensionsDllDirector::PostCityInit_(
    const cIGZMessage2Standard* pStandardMsg)
{
    // Resolve view3d
    cISC4AppPtr pSC4App;
    if (pSC4App) {
        constexpr uint32_t kGZWin_WinSC4App    = 0x6104489A;
        constexpr uint32_t kGZWin_SC4View3DWin  = 0x9a47b417;
        constexpr uint32_t kGZIID_cISC4View3DWin = 0xFA47B3F9;

        cIGZWin* pMainWindow = pSC4App->GetMainWindow();
        if (pMainWindow) {
            cIGZWin* pWinSC4App = pMainWindow->GetChildWindowFromID(kGZWin_WinSC4App);
            if (pWinSC4App) {
                pWinSC4App->GetChildAs(
                    kGZWin_SC4View3DWin,
                    kGZIID_cISC4View3DWin,
                    reinterpret_cast<void**>(&view3d_));
            }
        }

        cISC4City* pCity = pSC4App->GetCity();
        if (pCity) {
            LOG_INFO("PostCityInit: base elevation = {}",
                     pCity->GetWorldBaseElevation());
        }
    }

    city_ = static_cast<cISC4City*>(pStandardMsg->GetVoid1());
    cISTETerrain* pTerrain = city_->GetTerrain();

    // Register cheats
    if (cheatCodeManager_) {
        cheatCodeManager_->AddNotification2(this, 0);
        cheatCodeManager_->RegisterCheatCode(
            kTerrainExtensionsCheatID,
            cRZBaseString(kTerrainExtensionsCheatString.data(),
                          kTerrainExtensionsCheatString.size()));
        cheatCodeManager_->RegisterCheatCode(
            kTerrainExtensionsBridgeCheatID,
            cRZBaseString(kTerrainExtensionsBridgeCheatString.data(),
                          kTerrainExtensionsBridgeCheatString.size()));
    } else {
        LOG_ERROR("PostCityInit: cheat code manager not initialized");
    }

    // Acquire services
    if (mpFrameWork->GetSystemService(
            kImGuiServiceID, GZIID_cIGZImGuiService,
            reinterpret_cast<void**>(&imguiService_)))
    {
        LOG_INFO("Acquired ImGui service");

        if (mpFrameWork->GetSystemService(
                kS3DCameraServiceID, GZIID_cIGZS3DCameraService,
                reinterpret_cast<void**>(&cameraService_))) {
            LOG_INFO("Acquired S3D camera service");
        } else {
            LOG_WARN("S3D camera service not available");
        }

        if (mpFrameWork->GetSystemService(
                kDrawServiceID, GZIID_cIGZDrawService,
                reinterpret_cast<void**>(&drawService_)))
        {
            LOG_INFO("Acquired Draw service");
            if (drawService_->RegisterDrawPassCallback(
                    DrawServicePass::PreDynamic,
                    &DrawOverlayCallback_, this,
                    &drawCallbackToken_))
            {
                LOG_INFO("Registered draw pass callback");
            } else {
                LOG_WARN("RegisterDrawPassCallback failed");
            }
        } else {
            LOG_WARN("Draw service not available");
        }
    } else {
        LOG_WARN("ImGui service not available");
    }

    // Build tool subsystems
    SetUpCommandTools_(city_, pTerrain);
    SetUpDragTools_(city_, view3d_);
}


void TerrainExtensionsDllDirector::SetUpCommandTools_(
    cISC4City* pCity, cISTETerrain* pTerrain)
{
    LOG_DEBUG("Setting up command tools...");

    toolRegistry_.RegisterTool(std::make_unique<ConstantGradeTool>(pTerrain));
    toolRegistry_.RegisterTool(std::make_unique<FlattenTool>(pTerrain));
    toolRegistry_.RegisterTool(std::make_unique<BridgeApproachCommand>(pTerrain));
    toolRegistry_.RegisterTool(std::make_unique<BlueprintCaptureTool>(pTerrain, pCity));
    toolRegistry_.RegisterTool(std::make_unique<BlueprintExportTool>(pTerrain));
    toolRegistry_.RegisterTool(std::make_unique<BlueprintStampTool>(pTerrain, pCity));

    toolRegistry_.ListTools();
    LOG_DEBUG("Command tools setup complete.");
}

void TerrainExtensionsDllDirector::SetUpDragTools_(
    cISC4City* pCity, cISC4View3DWin* pView3D)
{
    LOG_DEBUG("Setting up drag tools...");

    // Register drag tools here. Director never touches concrete types again.
    dragToolManager_.Register(std::make_unique<BridgeApproachDragTool>());
    // future: dragToolManager_.Register(std::make_unique<FlattenDragTool>());

    LOG_DEBUG("Drag tools setup complete.");
}

void TerrainExtensionsDllDirector::PreCityShutdown_(
    cIGZMessage2Standard* pStandardMsg)
{
    dragToolManager_.DeactivateAll();

    if (drawService_ && drawCallbackToken_) {
        drawService_->UnregisterDrawPassCallback(drawCallbackToken_);
        drawCallbackToken_ = 0;
    }

    cISC4View3DWin* localView3D = view3d_;
    view3d_ = nullptr;
    if (localView3D) {
        localView3D->Release();
    }
}

void TerrainExtensionsDllDirector::ProcessCheat_(
    cIGZMessage2Standard* pStandardMsg)
{
    const auto cheatID = static_cast<uint32_t>(pStandardMsg->GetData1());

    // Try drag tools first — each tool knows its own cheat ID
    if (dragToolManager_.TryActivate(
            cheatID, city_, view3d_, winMgr_,
            imguiService_, overlayDrawManager_))
    {
        return;
    }

    // Fall through to earthbender CLI
    if (cheatID != kTerrainExtensionsCheatID) return;

    const auto* pCheatString =
        static_cast<const cIGZString*>(pStandardMsg->GetVoid2());
    const std::string_view cheatStringView(
        pCheatString->Data(), pCheatString->Strlen());

    LOG_INFO("Cheat: {} (ID: 0x{:X})", cheatStringView.data(), cheatID);

    std::vector<std::string> tokens = SplitString_(std::string(cheatStringView));
    std::vector<char*> argv;
    argv.reserve(tokens.size());
    for (auto& t : tokens) {
        argv.push_back(const_cast<char*>(t.c_str()));
    }

    args::ArgumentParser parser("Terrain extension tools");
    args::Group commands(parser, "commands");
    toolRegistry_.RegisterAllArguments(commands);
    args::HelpFlag help(parser, "help", "Show this help menu", {"help"});

    try {
        parser.ParseCLI(static_cast<int>(argv.size()), argv.data());
        if (!toolRegistry_.ExecuteAnyTool(parser)) {
            ShowMessageBox_("Earthbender help", parser.Help());
        }
    }
    catch (args::Help&) {
        ShowMessageBox_("Earthbender help", parser.Help());
    }
    catch (args::ParseError& e) {
        ShowMessageBox_("Earthbender parse error",
            std::string("Parse error: ") + e.what() + "\n\n" + parser.Help());
        LOG_DEBUG("Parse error: {}", e.what());
    }
    catch (args::ValidationError& e) {
        ShowMessageBox_("Earthbender validation error",
            std::string("Validation error: ") + e.what() + "\n\n" + parser.Help());
        LOG_DEBUG("Validation error: {}", e.what());
    }
}

// ── Draw callback ─────────────────────────────────────────────────────────────

void TerrainExtensionsDllDirector::DrawOverlayCallback_(
    const DrawServicePass pass, const bool begin, void* pThis)
{
    if (pass != DrawServicePass::PreDynamic || begin) return;

    const auto pDirector = static_cast<TerrainExtensionsDllDirector*>(pThis);
    IDirect3DDevice7* device = nullptr;
    IDirectDraw7*     dd     = nullptr;

    if (pDirector->imguiService_->AcquireD3DInterfaces(&device, &dd)) {
        pDirector->overlayDrawManager_.DrawAll(device);
        device->Release();
        dd->Release();
    }
}

// ── Utilities ─────────────────────────────────────────────────────────────────

void TerrainExtensionsDllDirector::ShowMessageBox_(
    const std::string& title, const std::string& message) const
{
    if (winMgr_) {
        winMgr_->GZMsgBox(
            cRZBaseString(message), cRZBaseString(title), 0, true, 0);
    } else {
        LOG_ERROR("No window manager for message box: {}", message);
    }
}

std::vector<std::string> TerrainExtensionsDllDirector::SplitString_(
    const std::string& input)
{
    std::vector<std::string> result;
    if (input.empty()) return result;

    std::istringstream iss(input);
    std::string token;
    result.reserve(8);
    while (iss >> token && result.size() < 32) {
        if (!token.empty()) result.push_back(token);
    }
    return result;
}

// ── Entry point ───────────────────────────────────────────────────────────────

static TerrainExtensionsDllDirector sDirector;

cRZCOMDllDirector* RZGetCOMDllDirector() {
    static auto sAddedRef = false;
    if (!sAddedRef) {
        sDirector.AddRef();
        sAddedRef = true;
    }
    return &sDirector;
}