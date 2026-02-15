/*
 * This file is part of sc4-terrain-extensions, a DLL Plugin for
 * SimCity 4 that offers some extra terrain utilities.
 *
 * Copyright (C) 2025 Casper Van Gheluwe
 *
 * sc4-terrain-extensions is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * sc4-terrain-extensions is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with sc4-terrain-extensions.
 * If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "cISC4ViewInputControl.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "cRZMessage2COMDirector.h"
#include "GZServPtrs.h"
#include "args.hxx"
#include "tools/TerrainToolRegistry.hpp"
#include "tools/ConstantGradeTool.cpp"
#include "tools/FlattenTool.cpp"
#include "tools/BridgeApproachTool.cpp"
#include "tools/BlueprintCaptureTool.cpp"
#include "tools/BlueprintExportTool.cpp"
#include "tools/BlueprintStampTool.cpp"
#include <sstream>
#include "controls/BridgeDragViewInputControl.cpp"
#include <windows.h>

#include "public/cIGZImGuiService.h"
#include "public/ImGuiPanelAdapter.h"
#include "public/ImGuiServiceIds.h"
#include "public/S3DCameraServiceIds.h"
#include "viz/BridgeApproachVisualizer.hpp"
#include "viz/BridgeToolPanel.hpp"

TerrainExtensionsDllDirector::TerrainExtensionsDllDirector()
	: pCheatCodeManager(nullptr),
	  pView3D(nullptr),
	  pCity(nullptr),
	  pWinMgr(nullptr),
	  pMS2(nullptr),
	  pImGui(nullptr),
      pCamera(nullptr),
      bPanelRegistered(false),
	  bPanelVisible(false),
      pDraw(nullptr),
	  nBridgeDrawCallbackToken(0) {
	Logger::Initialize("SC4TerrainExtensions");

	mToolRegistry = TerrainToolRegistry();

	LOG_INFO("SC4TerrainExtensions v{}", PLUGIN_VERSION_STR);
}

TerrainExtensionsDllDirector::~TerrainExtensionsDllDirector() {
	LOG_INFO("~TerrainExtensionsDllDirector()");
	Logger::Shutdown();
}

uint32_t TerrainExtensionsDllDirector::GetDirectorID() const {
	return kTerrainExtensionsDirectorID;
}

bool TerrainExtensionsDllDirector::OnStart(cIGZCOM* pCOM) {
	mpFrameWork->AddHook(this);
	return true;
}


bool TerrainExtensionsDllDirector::DoMessage(cIGZMessage2* pMsg) {
	auto pStandardMsg = static_cast<cIGZMessage2Standard*>(pMsg);

	switch (pMsg->GetType()) {
	case kMessageCheatIssued:
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


void TerrainExtensionsDllDirector::PostCityInit_(const cIGZMessage2Standard* pStandardMsg) {
	cISC4AppPtr pSC4App;
	cIGZMessageServer2Ptr pMessageServer;
	cIGZApp* pApp = mpFrameWork->Application();

	if (pSC4App && pMessageServer) {
		constexpr uint32_t kGZWin_WinSC4App = 0x6104489A;
		constexpr uint32_t kGZWin_SC4View3DWin = 0x9a47b417;
		constexpr uint32_t kGZIID_cISC4View3DWin = 0xFA47B3F9;

		cIGZWin* pMainWindow = pSC4App->GetMainWindow();

		if (pMainWindow) {
			cIGZWin* pWinSC4App = pMainWindow->GetChildWindowFromID(kGZWin_WinSC4App);

			if (pWinSC4App) {
				if (pWinSC4App->GetChildAs(
					kGZWin_SC4View3DWin,
					kGZIID_cISC4View3DWin,
					reinterpret_cast<void**>(&pView3D))) {
					cISC4City* pCity = pSC4App->GetCity();

					if (pCity) {
						LOG_INFO("PostCityInit: City initialized with base elevation: {}",
						         pCity->GetWorldBaseElevation());
					}
				}
			}
		}
	}

	pCity = static_cast<cISC4City*>(pStandardMsg->GetVoid1());
	SetUpTools_(pCity, pCity->GetTerrain());

	if (pCheatCodeManager) {
		pCheatCodeManager->AddNotification2(this, 0);
		pCheatCodeManager->RegisterCheatCode(
			kTerrainExtensionsCheatID,
			cRZBaseString(
				kTerrainExtensionsCheatString.data(),
				kTerrainExtensionsCheatString.size()
			)
		);
		pCheatCodeManager->RegisterCheatCode(
			kTerrainExtensionsBridgeCheatID,
			cRZBaseString(
				kTerrainExtensionsBridgeCheatString.data(),
				kTerrainExtensionsBridgeCheatString.size()
			)
		);
	}
	else {
		LOG_ERROR("PostCityInit: Cheat code manager is not initialized.");
	}
	LOG_DEBUG("PostCityInit: Cheat code manager initialized with cheat ID: 0x{:X}", kTerrainExtensionsCheatID);

	if (mpFrameWork && mpFrameWork->GetSystemService(kImGuiServiceID, GZIID_cIGZImGuiService, reinterpret_cast<void**>(&pImGui))
	) {
		spdlog::info("Acquired ImGui service");
		gImGuiService = pImGui;

		if (mpFrameWork->GetSystemService(kS3DCameraServiceID, GZIID_cIGZS3DCameraService, reinterpret_cast<void**>(&pCamera))) {
			spdlog::info("Acquired S3D camera service");
		}
		else {
			spdlog::warn("S3D camera service not available");
		}

		if (mpFrameWork->GetSystemService(kDrawServiceID, GZIID_cIGZDrawService, reinterpret_cast<void**>(&pDraw))) {
			spdlog::info("Acquired Draw service");

			if (!pDraw->RegisterDrawPassCallback(DrawServicePass::PreDynamic, &DrawBridgeVisualizerCallback_,nullptr, &nBridgeDrawCallbackToken)) {
				LOG_WARN("RegisterDrawPassCallback failed");
			}
		} else {
			spdlog::warn("Draw service not found");
		}

		pPanel = std::make_unique<BridgeToolPanel>(this, pImGui);
		const ImGuiPanelDesc desc = ImGuiPanelAdapter<BridgeToolPanel>::MakeDesc(
			pPanel.get(), kBridgeToolPanelId, 100, true
		);

		if (pImGui->RegisterPanel(desc)) {
			bPanelRegistered = true;
			bPanelVisible = true;
			pPanel->SetOpen(true);
			spdlog::info("Registered ImGui panel");
		}
	}
	else {
		spdlog::warn("ImGui service not found or not available");
	}
}

void TerrainExtensionsDllDirector::SetUpTools_(cISC4City* pCityIn, cISTETerrain* pTerrain) {
	LOG_DEBUG("Setting up terrain tools...");

	mToolRegistry.RegisterTool(std::make_unique<ConstantGradeTool>(pTerrain));
	mToolRegistry.RegisterTool(std::make_unique<FlattenTool>(pTerrain));
	mToolRegistry.RegisterTool(std::make_unique<BridgeApproachTool>(pTerrain));
	mToolRegistry.RegisterTool(std::make_unique<BlueprintCaptureTool>(pTerrain, pCityIn));
	mToolRegistry.RegisterTool(std::make_unique<BlueprintExportTool>(pTerrain));
	mToolRegistry.RegisterTool(std::make_unique<BlueprintStampTool>(pTerrain, pCityIn));

	LOG_DEBUG("Terrain tools setup complete.");
	mToolRegistry.ListTools();
}

void TerrainExtensionsDllDirector::ActivateBridgeDragMode_() {
	const auto bridgeControl = new BridgeDragViewInputControl(pCity->GetTerrain(), pWinMgr->GetMainWindow(),
	                                                          pView3D);
	bridgeControl->Init();
	if (bridgeControl) {
		ActivateDragControl_(bridgeControl);
	}
}

bool TerrainExtensionsDllDirector::ActivateDragControl_(BaseDragViewInputControl* control) {
	if (!pView3D || !control) return false;

	// If we already have an active control, deactivate it first
	if (mActiveDragControl) {
		mActiveDragControl->Deactivate();
		mActiveDragControl = nullptr;
	}

	mActiveDragControl = control;
	if (mActiveDragControl->Init()) {
		LOG_DEBUG("Activating drag control: {}", static_cast<void*>(mActiveDragControl));
		pView3D->SetCurrentViewInputControl(mActiveDragControl,
		                                    cISC4View3DWin::ViewInputControlStackOperation_None);
		LOG_DEBUG("Activated drag control: {}", static_cast<void*>(mActiveDragControl));
		return true;
	}

	mActiveDragControl = nullptr;
	return false;
}


void TerrainExtensionsDllDirector::ShowMessageBox_(const std::string& title, const std::string& message) const {
	if (pWinMgr) {
		const cRZBaseString titleStr(title);
		const cRZBaseString messageStr(message);
		pWinMgr->GZMsgBox(messageStr, titleStr, 0, true, 0);
	}
	else {
		LOG_ERROR("Failed to get window manager for message box with message: {}", message);
	}
}

void TerrainExtensionsDllDirector::DrawBridgeVisualizerCallback_(DrawServicePass pass, bool begin, void*) {
	if (pass != DrawServicePass::PreDynamic || begin) return;

	if (!gImGuiService) return;

	IDirect3DDevice7* device = nullptr;
	IDirectDraw7* dd = nullptr;
	if (gImGuiService->AcquireD3DInterfaces(&device, &dd)) {
		gBridgeVisualizer.Draw(device);
		device->Release();
		dd->Release();
	}
}

static std::vector<std::string> SplitString(const std::string& input) {
	std::vector<std::string> result;
	if (input.empty()) return result;

	std::istringstream iss(input);
	std::string token;
	result.reserve(8);

	while (iss >> token && result.size() < 32) {
		if (!token.empty()) {
			result.push_back(token);
		}
	}
	return result;
}

void TerrainExtensionsDllDirector::ProcessCheat_(cIGZMessage2Standard* pStandardMsg) {
	const uint32_t cheatID = static_cast<uint32_t>(pStandardMsg->GetData1());

	if (cheatID == kTerrainExtensionsBridgeCheatID) {
		LOG_DEBUG("Bridge approach cheat code issued");
		ActivateBridgeDragMode_();
		return;
	}
	if (cheatID == kTerrainExtensionsCheatID) {
		const auto* pCheatString = static_cast<const cIGZString*>(pStandardMsg->GetVoid2());
		const std::string_view cheatStringView(pCheatString->Data(), pCheatString->Strlen());

		LOG_INFO("Cheat code issued: {} (ID: 0x{:X})", cheatStringView.data(), cheatID);

		std::vector<std::string> tokens = SplitString(std::string(cheatStringView));

		LOG_DEBUG("Parsing {} tokens:", tokens.size());
		for (size_t i = 0; i < tokens.size(); ++i) {
			LOG_DEBUG("  Token[{}]: '{}'", i, tokens[i].c_str());
		}

		std::vector<char*> argv;
		argv.reserve(tokens.size()); // Prevent reallocation

		for (size_t i = 0; i < tokens.size(); ++i) {
			argv.push_back(const_cast<char*>(tokens[i].c_str())); // Use c_str() for stability
		}

		args::ArgumentParser parser("Terrain extension tools");
		args::Group commands(parser, "commands");

		mToolRegistry.RegisterAllArguments(commands);
		args::HelpFlag help(parser, "help", "Show this help menu", {"help"});
		try {
			parser.ParseCLI(argv.size(), argv.data());

			if (!mToolRegistry.ExecuteAnyTool(parser)) {
				ShowMessageBox_("Earthbender help", parser.Help());
				LOG_DEBUG("No matching command found, showing help");
			}
		}
		catch (args::Help&) {
			ShowMessageBox_("Earthbender help", parser.Help());
		}
		catch (args::ParseError& e) {
			std::string errorMsg = "Command parse error: ";
			errorMsg += e.what();
			errorMsg += "\n\n";
			errorMsg += parser.Help();
			ShowMessageBox_("Earthbender parse error", errorMsg);
			LOG_DEBUG("Command parse error: {}", e.what());
		}
		catch (args::ValidationError& e) {
			std::string errorMsg = "Command validation error: ";
			errorMsg += e.what();
			errorMsg += "\n\n";
			errorMsg += parser.Help();
			ShowMessageBox_("Earthbender validation error", errorMsg);
			LOG_DEBUG("Command validation error: {}", e.what());
		}
	}
}

bool TerrainExtensionsDllDirector::PostAppInit() {
	cIGZMessageServer2Ptr pMS2;
	LOG_INFO("PostAppInit: Initializing TerrainExtensionsDllDirector");

	cIGZApp* const pApp = mpFrameWork->Application();

	if (pApp) {
		cRZAutoRefCount<cISC4App> pSC4App;

		if (pApp->QueryInterface(GZIID_cISC4App, pSC4App.AsPPVoid())) {
			pCheatCodeManager = pSC4App->GetCheatCodeManager();
			pWinMgr = pSC4App->GetMainWindow()->GetWindowManager();
		}
	}

	if (pMS2) {
		pMS2->AddNotification(this, kSC4MessagePostCityInit);
		pMS2->AddNotification(this, kSC4MessagePreCityShutdown);
		this->pMS2 = pMS2;
	}

	return true;
}

void TerrainExtensionsDllDirector::PreCityShutdown_(cIGZMessage2Standard* pStandardMsg) {
	cISC4View3DWin* localView3D = pView3D;
	pView3D = nullptr;

	if (localView3D) {
		localView3D->Release();
	}
}

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static TerrainExtensionsDllDirector sDirector;
	return &sDirector;
}
