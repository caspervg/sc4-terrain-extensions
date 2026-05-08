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
#include "tools/constantgrade/ConstantGradeInteractiveTool.hpp"
#include "tools/flatten/FlattenInteractiveTool.hpp"
#include "tools/BlueprintCaptureTool.cpp"
#include "tools/BlueprintExportTool.cpp"
#include "tools/BlueprintStampTool.cpp"
#include "tools/ContourMapTool.hpp"
#include "tools/SlopeMapTool.hpp"
#include "tools/SlopeMapTool.cpp"

#include "tools/bridge/BridgeApproachDragTool.hpp"

#include "snapshot/SnapshotDragTool.hpp"
#include "snapshot/SnapshotPanel.hpp"

#include "public/cIGZImGuiService.h"
#include "public/cIGZS3DCameraService.h"
#include "public/ImGuiPanelAdapter.h"
#include "public/ImGuiServiceIds.h"
#include "public/S3DCameraServiceIds.h"
#include "ui/TerrainCatalogHook.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <new>
#include <sstream>
#include <d3d.h>
#include <ddraw.h>
#include <windows.h>

#include "controls/StatefulDragViewInputControl.hpp"
#include "public/cIGZDrawService.h"
#include "viz/D3D7StateGuard.hpp"
#include "viz/TerrainSlopeRenderer.cpp"


static constexpr uint32_t kTerrainExtensionsDirectorID     = 0x2099E7AB; // your actual ID
static constexpr uint32_t kTerrainExtensionsCheatID        = 0x1234ABCD; // your actual ID
static constexpr uint32_t kTerrainExtensionsBridgeCheatID  = 0x9773F4CD; // your actual ID
static constexpr std::string_view kTerrainExtensionsCheatString = "earthbender";
static constexpr std::string_view kTerrainExtensionsBridgeCheatString = "bridgebuilder";
static constexpr uint32_t kTerrainExtensionsSnapshotCheatID = 0x7E5A9B00;
static constexpr std::string_view kTerrainExtensionsSnapshotCheatString = "terrainsnap";
static constexpr uint32_t kTerrainExtensionsContourCheatID = 0x7E5A9B10;
static constexpr std::string_view kTerrainExtensionsContourCheatString = "contour";
static constexpr uint32_t kTerrainExtensionsSlopeCheatID = 0x7E5A9B20;
static constexpr std::string_view kTerrainExtensionsSlopeCheatString = "slope";
static constexpr uint32_t kSC4MessageCheatIssued = 0x230E27AC;
static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

namespace {
enum class OverlayCameraResetStatus {
    DrawServiceMissing = 0,
    DrawContextMissing,
    CameraServiceMissing,
    ActiveCameraApplied,
    ModelViewFallback,
    Count
};

const char* OverlayCameraResetStatusName(const OverlayCameraResetStatus status) {
    switch (status) {
    case OverlayCameraResetStatus::DrawServiceMissing:
        return "draw-service-missing";
    case OverlayCameraResetStatus::DrawContextMissing:
        return "draw-context-missing";
    case OverlayCameraResetStatus::CameraServiceMissing:
        return "camera-service-missing";
    case OverlayCameraResetStatus::ActiveCameraApplied:
        return "active-camera-applied";
    case OverlayCameraResetStatus::ModelViewFallback:
        return "model-view-fallback";
    default:
        return "unknown";
    }
}

void LogOverlayCameraResetStatus(
    const OverlayCameraResetStatus status,
    const void* drawContext,
    const void* camera) {
    static bool sLogged[static_cast<int>(OverlayCameraResetStatus::Count)]{};
    const auto index = static_cast<int>(status);
    if (index < 0 || index >= static_cast<int>(OverlayCameraResetStatus::Count) || sLogged[index]) {
        return;
    }

    sLogged[index] = true;
    LOG_DEBUG(
        "DrawOverlayCallback_: camera reset path={} drawContext={} camera={}",
        OverlayCameraResetStatusName(status),
        drawContext,
        camera);
}

struct ContourLabelRenderPayload {
    cIGZS3DCameraService* cameraService{};
    std::vector<TerrainContourRenderer::LabelAnchor> labels{};
};

void CleanupContourLabelsImGui(void* data) {
    auto* payload = static_cast<ContourLabelRenderPayload*>(data);
    if (!payload) return;
    delete payload;
}

void RenderContourLabelsImGui(void* data) {
    auto* payload = static_cast<ContourLabelRenderPayload*>(data);
    if (!payload || !payload->cameraService || payload->labels.empty()) {
        return;
    }

    const S3DCameraHandle cameraHandle = payload->cameraService->WrapActiveRendererCamera();
    if (!cameraHandle.ptr) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) {
        payload->cameraService->DestroyCamera(cameraHandle);
        return;
    }

    constexpr ImU32 kOutlineColor = IM_COL32(0, 0, 0, 190);
    constexpr uint8_t kBaseAlpha = 230;
    constexpr float kLabelFontScale = 1.15f;

    for (const auto& label : payload->labels) {
        float screenX = 0.0f;
        float screenY = 0.0f;
        float depth = 0.0f;
        if (!payload->cameraService->WorldToScreen(
            cameraHandle,
            label.worldX, label.worldY, label.worldZ,
            screenX, screenY, &depth)) {
            continue;
        }

        const float fade = std::clamp(1.0f / (1.0f + depth * 0.0015f), 0.35f, 1.0f);
        const uint8_t alpha = static_cast<uint8_t>(std::clamp(fade * kBaseAlpha, 40.0f, 255.0f));
        const ImU32 textColor = IM_COL32(248, 248, 248, alpha);

        ImFont* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize() * kLabelFontScale;
        const ImVec2 size = font
            ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.text.c_str())
            : ImGui::CalcTextSize(label.text.c_str());
        const ImVec2 pos(screenX - size.x * 0.5f, screenY - size.y * 0.5f);

        drawList->AddText(font, fontSize, ImVec2(pos.x + 1.0f, pos.y), kOutlineColor, label.text.c_str());
        drawList->AddText(font, fontSize, ImVec2(pos.x - 1.0f, pos.y), kOutlineColor, label.text.c_str());
        drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y + 1.0f), kOutlineColor, label.text.c_str());
        drawList->AddText(font, fontSize, ImVec2(pos.x, pos.y - 1.0f), kOutlineColor, label.text.c_str());
        drawList->AddText(font, fontSize, pos, textColor, label.text.c_str());
    }

    payload->cameraService->DestroyCamera(cameraHandle);
}
}

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

TerrainExtensionsDllDirector::~TerrainExtensionsDllDirector() {
    TerrainCatalogHook::Remove();
}

uint32_t TerrainExtensionsDllDirector::GetDirectorID() const {
    return kTerrainExtensionsDirectorID;
}

bool TerrainExtensionsDllDirector::OnStart(cIGZCOM* pCOM) {
    mpFrameWork->AddHook(this);
    TerrainCatalogHook::Install(*this);
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
        cheatCodeManager_->RegisterCheatCode(
            kTerrainExtensionsSnapshotCheatID,
            cRZBaseString(kTerrainExtensionsSnapshotCheatString.data(),
                          kTerrainExtensionsSnapshotCheatString.size()));
        cheatCodeManager_->RegisterCheatCode(
            kTerrainExtensionsContourCheatID,
            cRZBaseString(kTerrainExtensionsContourCheatString.data(),
                          kTerrainExtensionsContourCheatString.size()));
        cheatCodeManager_->RegisterCheatCode(
            kTerrainExtensionsSlopeCheatID,
            cRZBaseString(kTerrainExtensionsSlopeCheatString.data(),
                          kTerrainExtensionsSlopeCheatString.size()));
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
                    DrawServicePass::PostDynamic,
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

    // Register independent contour renderer (not tied to snapshot panel)
    overlayDrawManager_.Register(&contourRenderer_);
    contourCommand_ = std::make_unique<ContourMapTool>(pTerrain, contourRenderer_);
    overlayDrawManager_.Register(&slopeRenderer_);
    slopeCommand_ = std::make_unique<SlopeMapTool>(pTerrain, slopeRenderer_);

    // Capture the pristine city-load state before any tools run
    snapshotManager_.Capture(pTerrain, "City load",
        "Initial terrain state captured at city load");

    // Set up snapshot panel
    if (imguiService_ && pTerrain) {
        // Register the snapshot renderer permanently so preview works from the panel
        overlayDrawManager_.Register(&snapshotRenderer_);

        snapshotPanel_ = std::make_unique<SnapshotPanel>(
            snapshotManager_, pTerrain, snapshotRenderer_,
            [this](int snapshotIndex) {
                // Partial restore callback: activate drag tool
                if (!snapshotDragTool_ || !city_ || !view3d_ || !winMgr_) return;
                snapshotDragTool_->SetRestoreIndex(snapshotIndex);
                snapshotDragTool_->ActivateDirect(city_, view3d_, winMgr_, overlayDrawManager_);
            });

        const auto desc = ImGuiPanelAdapter<SnapshotPanel>::MakeDesc(
            snapshotPanel_.get(), SnapshotPanel::kPanelId, 100, false);

        if (imguiService_->RegisterPanel(desc)) {
            snapshotPanelRegistered_ = true;
            snapshotPanelVisible_ = false;
            LOG_INFO("Registered snapshot panel");
        } else {
            LOG_WARN("Failed to register snapshot panel");
        }
    }
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

    // Register drag tools here.
    dragToolManager_.Register(std::make_unique<FlattenInteractiveTool>());
    dragToolManager_.Register(std::make_unique<ConstantGradeInteractiveTool>());
    dragToolManager_.Register(std::make_unique<BridgeApproachDragTool>());

    auto snapshotTool = std::make_unique<SnapshotDragTool>(snapshotManager_, snapshotRenderer_);
    snapshotDragTool_ = snapshotTool.get();
    dragToolManager_.Register(std::move(snapshotTool));

    LOG_DEBUG("Drag tools setup complete.");
}

void TerrainExtensionsDllDirector::PreCityShutdown_(
    cIGZMessage2Standard* pStandardMsg)
{
    dragToolManager_.DeactivateAll();
    dragToolManager_.Clear();
    toolRegistry_.Clear();

    // Clean up snapshot system
    if (imguiService_ && snapshotPanelRegistered_) {
        imguiService_->UnregisterPanel(SnapshotPanel::kPanelId);
        snapshotPanelRegistered_ = false;
        snapshotPanelVisible_ = false;
    }
    snapshotPanel_.reset();
    snapshotManager_.Clear();
    snapshotRenderer_.ClearAll();
    contourRenderer_.SetEnabled(false, nullptr);
    contourCommand_.reset();
    slopeRenderer_.SetEnabled(false, nullptr);
    slopeCommand_.reset();
    overlayDrawManager_.Unregister(&snapshotRenderer_);
    overlayDrawManager_.Unregister(&contourRenderer_);
    overlayDrawManager_.Unregister(&slopeRenderer_);
    snapshotDragTool_ = nullptr;

    if (drawService_ && drawCallbackToken_) {
        drawService_->UnregisterDrawPassCallback(drawCallbackToken_);
        drawCallbackToken_ = 0;
    }

    cISC4View3DWin* localView3D = view3d_;
    view3d_ = nullptr;
    city_ = nullptr;
    if (localView3D) {
        localView3D->Release();
    }
}

void TerrainExtensionsDllDirector::ProcessCheat_(
    cIGZMessage2Standard* pStandardMsg)
{
    const auto cheatID = static_cast<uint32_t>(pStandardMsg->GetData1());
    const auto* pCheatString =
        static_cast<const cIGZString*>(pStandardMsg->GetVoid2());
    const std::string_view cheatStringView(
        pCheatString->Data(), pCheatString->Strlen());

    // Handle snapshot panel toggle
    if (cheatID == kTerrainExtensionsSnapshotCheatID) {
        if (imguiService_ && snapshotPanelRegistered_) {
            snapshotPanelVisible_ = !snapshotPanelVisible_;
            imguiService_->SetPanelVisible(SnapshotPanel::kPanelId, snapshotPanelVisible_);
            LOG_INFO("Snapshot panel {}", snapshotPanelVisible_ ? "shown" : "hidden");
        }
        return;
    }

    // Handle top-level contour command
    if (cheatID == kTerrainExtensionsContourCheatID) {
        if (contourCommand_) {
            const auto result = contourCommand_->ExecuteCommand(SplitString_(std::string(cheatStringView)));
            if (!result.title.empty() && !result.message.empty()) {
                ShowMessageBox_(result.title, result.message);
            }
        } else {
            ShowMessageBox_("Contour error", "Contour commands are not available right now.");
        }
        return;
    }

    if (cheatID == kTerrainExtensionsSlopeCheatID) {
        if (slopeCommand_) {
            const auto result = slopeCommand_->ExecuteCommand(SplitString_(std::string(cheatStringView)));
            if (!result.title.empty() && !result.message.empty()) {
                ShowMessageBox_(result.title, result.message);
            }
        } else {
            ShowMessageBox_("Slope error", "Slope commands are not available right now.");
        }
        return;
    }

    // Try drag tools first — each tool knows its own cheat ID
    if (dragToolManager_.TryActivate(
            cheatID, city_, view3d_, winMgr_,
            imguiService_, overlayDrawManager_, &snapshotManager_))
    {
        return;
    }

    // Fall through to earthbender CLI
    if (cheatID != kTerrainExtensionsCheatID) return;

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

bool TerrainExtensionsDllDirector::HandleCustomTerrainCatalogItem(
    const uint32_t itemId,
    cISC4View3DWin* sourceView3D,
    const bool activateTool)
{
    view3d_ = sourceView3D ? sourceView3D : view3d_;

    if (!city_ || !view3d_ || !winMgr_) {
        LOG_WARN(
            "Terrain catalog item 0x{:08X} ignored because tool context is incomplete (city={}, view3d={}, winMgr={})",
            itemId,
            static_cast<const void*>(city_),
            static_cast<const void*>(view3d_),
            static_cast<const void*>(winMgr_));
        return true;
    }

    switch (itemId) {
    case TerrainCatalogHook::ItemId::Flatten:
        LOG_INFO("Terrain catalog item 0x{:08X}: activating flatten tool", itemId);
        return dragToolManager_.TryActivate(
            TerrainCatalogHook::ItemId::Flatten,
            city_, view3d_, winMgr_, imguiService_,
            overlayDrawManager_, &snapshotManager_);

    case TerrainCatalogHook::ItemId::BridgeApproach:
        LOG_INFO("Terrain catalog item 0x{:08X}: activating bridge approach tool", itemId);
        return dragToolManager_.TryActivate(
            kTerrainExtensionsBridgeCheatID,
            city_, view3d_, winMgr_, imguiService_,
            overlayDrawManager_, &snapshotManager_);

    case TerrainCatalogHook::ItemId::ConstantGrade:
        LOG_INFO("Terrain catalog item 0x{:08X}: activating constant grade tool", itemId);
        return dragToolManager_.TryActivate(
            TerrainCatalogHook::ItemId::ConstantGrade,
            city_, view3d_, winMgr_, imguiService_,
            overlayDrawManager_, &snapshotManager_);
    case TerrainCatalogHook::ItemId::BlueprintCapture:
    case TerrainCatalogHook::ItemId::BlueprintExport:
    case TerrainCatalogHook::ItemId::BlueprintStamp:
        LOG_INFO(
            "Terrain catalog item 0x{:08X} clicked (activateTool={}) but no menu activation bridge exists yet",
            itemId,
            activateTool);
        ShowMessageBox_(
            "Terrain Extensions",
            "The selected terrain catalog item is hooked correctly, but this tool is not wired to a menu activation flow yet.");
        return true;

    default:
        return false;
    }
}

// ── Draw callback ─────────────────────────────────────────────────────────────

void TerrainExtensionsDllDirector::DrawOverlayCallback_(
    const DrawServicePass pass, const bool begin, void* pThis)
{
    if (pass != DrawServicePass::PostDynamic || begin) return;

    const auto pDirector = static_cast<TerrainExtensionsDllDirector*>(pThis);
    IDirect3DDevice7* device = nullptr;
    IDirectDraw7*     dd     = nullptr;

    if (pDirector->imguiService_ && pDirector->imguiService_->AcquireD3DInterfaces(&device, &dd)) {
        const bool needsSlopeUpdate =
            pDirector->city_
            && pDirector->cameraService_
            && pDirector->slopeRenderer_.IsEnabled();
        const bool needsOverlayDraw =
            needsSlopeUpdate || pDirector->overlayDrawManager_.HasVisibleGeometry();

        if (needsOverlayDraw) {
            D3D7StateGuard guard(device);
            if (pDirector->drawService_) {
                const SC4DrawContextHandle drawContext =
                    pDirector->drawService_->WrapActiveRendererDrawContext();
                if (drawContext.ptr) {
                    pDirector->drawService_->SetDefaultRenderStateUnilaterally(drawContext);
                    if (pDirector->cameraService_) {
                        const S3DCameraHandle camera =
                            pDirector->cameraService_->WrapActiveRendererCamera();
                        if (camera.ptr) {
                            pDirector->drawService_->SetCamera(
                                drawContext,
                                reinterpret_cast<cS3DCamera*>(camera.ptr));
                            LogOverlayCameraResetStatus(
                                OverlayCameraResetStatus::ActiveCameraApplied,
                                drawContext.ptr,
                                camera.ptr);
                        } else {
                            pDirector->drawService_->SetModelViewTransformChanged(drawContext, 0);
                            pDirector->drawService_->ResetModelViewTransform(drawContext);
                            LogOverlayCameraResetStatus(
                                OverlayCameraResetStatus::ModelViewFallback,
                                drawContext.ptr,
                                nullptr);
                        }
                    } else {
                        LogOverlayCameraResetStatus(
                            OverlayCameraResetStatus::CameraServiceMissing,
                            drawContext.ptr,
                            nullptr);
                    }
                } else {
                    LogOverlayCameraResetStatus(
                        OverlayCameraResetStatus::DrawContextMissing,
                        nullptr,
                        nullptr);
                }
            } else {
                LogOverlayCameraResetStatus(
                    OverlayCameraResetStatus::DrawServiceMissing,
                    nullptr,
                    nullptr);
            }
            if (needsSlopeUpdate) {
                pDirector->slopeRenderer_.UpdateView(
                    pDirector->city_->GetTerrain(),
                    pDirector->cameraService_,
                    device);
            }
            pDirector->overlayDrawManager_.DrawAll(device);
        }
        device->Release();
        dd->Release();
    } else {
        LOG_WARN("DrawOverlayCallback_: AcquireD3DInterfaces failed, skipping overlay draw");
    }

    if (pDirector->imguiService_
        && pDirector->cameraService_
        && pDirector->contourRenderer_.IsEnabled())
    {
        const auto& labels = pDirector->contourRenderer_.GetLabelAnchors();
        if (!labels.empty()) {
            auto* payload = new (std::nothrow) ContourLabelRenderPayload();
            if (payload) {
                payload->cameraService = pDirector->cameraService_;
                payload->labels = labels; // Snapshot to avoid cross-frame mutations.

                if (!pDirector->imguiService_->QueueRender(
                    &RenderContourLabelsImGui,
                    payload,
                    &CleanupContourLabelsImGui))
                {
                    CleanupContourLabelsImGui(payload);
                }
            }
        }
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
