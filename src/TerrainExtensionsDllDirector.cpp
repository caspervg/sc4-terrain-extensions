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

#include "version.h"
#include "cGZPersistResourceKey.h"
#include "Logger.h"
#include "FileSystem.h"
#include "cIGZApp.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZMessage2Standard.h"
#include "cIGZMessageServer2.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZWin.h"
#include "cIGZWinKeyAccelerator.h"
#include "cIGZWinKeyAcceleratorRes.h"
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
#include "TerrainToolRegistry.hpp"
#include "ConstantGradeTool.cpp"
#include "FlattenTool.cpp"
#include "BridgeApproachTool.cpp"
#include "TunnelApproachTool.cpp"
#include <sstream>
#include "Patcher.h"
#include "BridgeDragViewInputControl.cpp"

static constexpr uint32_t kMessageCheatIssued = 0x230E27AC;
static constexpr uint32_t kSC4MessagePostCityInit = 0x26D31EC1;
static constexpr uint32_t kSC4MessagePreCityShutdown = 0x26D31EC2;

static constexpr uint32_t kTerrainExtensionsDirectorID = 0xA20FD558;	// Randomly generated ID to avoid conflicts with other mods

static constexpr uint32_t kTerrainExtensionsCheatID = 0x903d4018;	// Randomly generated ID for the cheat code to avoid conflicts with other mods
static constexpr std::string_view kTerrainExtensionsCheatString = "earthbender";

static constexpr uint32_t kTerrainExtensionsBridgeCheatID = 0x9773F4CD;
static constexpr std::string_view kTerrainExtensionsBridgeCheatString = "bridgebuilder";


class TerrainExtensionsDllDirector final : public cRZMessage2COMDirector
{
public:
	TerrainExtensionsDllDirector()
		: pCheatCodeManager(nullptr),
		  pCity(nullptr),
		  pView3D(nullptr),
		  pWinMgr(nullptr)
	{
		Logger& logger = Logger::GetInstance();

		mToolRegistry = TerrainToolRegistry();

		logger.Init(FileSystem::GetLogFilePath(), LogLevel::Info);
		logger.WriteLogFileHeader("SC4TerrainExtensions v" PLUGIN_VERSION_STR);
	}

	uint32_t GetDirectorID() const
	{
		return kTerrainExtensionsDirectorID;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		mpFrameWork->AddHook(this);
		return true;
	}

private:
	cIGZCheatCodeManager* pCheatCodeManager;
	cISC4View3DWin* pView3D;
	cISC4City* pCity;
	cIGZWinMgr* pWinMgr;
	TerrainToolRegistry mToolRegistry;
	cRZAutoRefCount<BaseDragViewInputControl> mActiveDragControl;
	cIGZMessageServer2* pMS2;

	bool DoMessage(cIGZMessage2* pMsg)
	{
		auto pStandardMsg = static_cast<cIGZMessage2Standard*>(pMsg);

		switch (pMsg->GetType())
		{
		case kMessageCheatIssued:
			ProcessCheat(pStandardMsg);
			break;
		case kSC4MessagePostCityInit:
			PostCityInit(pStandardMsg);
			break;
		case kSC4MessagePreCityShutdown:
			PreCityShutdown(pStandardMsg);
			break;
		}

		return true;
	}


	void PostCityInit(cIGZMessage2Standard* pStandardMsg)
	{
		Logger& logger = Logger::GetInstance();

		cISC4AppPtr pSC4App;
		cIGZMessageServer2Ptr pMessageServer;
		cIGZApp* pApp = mpFrameWork->Application();

		if (pSC4App && pMessageServer)
		{
			constexpr uint32_t kGZWin_WinSC4App = 0x6104489A;
			constexpr uint32_t kGZWin_SC4View3DWin = 0x9a47b417;
			constexpr uint32_t kGZIID_cISC4View3DWin = 0xFA47B3F9;

			cIGZWin* pMainWindow = pSC4App->GetMainWindow();

			if (pMainWindow)
			{
				cIGZWin* pWinSC4App = pMainWindow->GetChildWindowFromID(kGZWin_WinSC4App);

				if (pWinSC4App)
				{
					if (pWinSC4App->GetChildAs(
						kGZWin_SC4View3DWin,
						kGZIID_cISC4View3DWin,
						reinterpret_cast<void**>(&pView3D)))
					{
						cISC4City* pCity = pSC4App->GetCity();

						if (pCity)
						{
							logger.WriteLineFormatted(LogLevel::Info, "PostCityInit: City initialized with base elevation: %f", pCity->GetWorldBaseElevation());
						}
					}
				}
			}
		}

		pCity = static_cast<cISC4City*>(pStandardMsg->GetVoid1());
		SetUpTools(pCity->GetTerrain());

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
			logger.WriteLine(LogLevel::Error, "PostCityInit: Cheat code manager is not initialized.");
		}
		logger.WriteLineFormatted(LogLevel::Info, "PostCityInit: Cheat code manager initialized with cheat ID: 0x%x", kTerrainExtensionsCheatID);
	}

	void SetUpTools(cISTETerrain* pTerrain) {
		Logger& logger = Logger::GetInstance();
		logger.WriteLine(LogLevel::Info, "Setting up terrain tools...");

		mToolRegistry.RegisterTool(std::make_unique<ConstantGradeTool>(pTerrain));
		mToolRegistry.RegisterTool(std::make_unique<FlattenTool>(pTerrain));
		mToolRegistry.RegisterTool(std::make_unique<BridgeApproachTool>(pTerrain));
		mToolRegistry.RegisterTool(std::make_unique<TunnelApproachTool>(pTerrain));

		logger.WriteLine(LogLevel::Info, "Terrain tools setup complete.");
		mToolRegistry.ListTools();
	}

	void ActivateBridgeDragMode() {
		auto bridgeControl = new BridgeDragViewInputControl(pCity->GetTerrain(), pWinMgr->GetMainWindow(), pView3D);
		if (bridgeControl) {
			ActivateDragControl(bridgeControl);
		}
	}

	bool ActivateDragControl(BaseDragViewInputControl* control) {
		if (!pView3D || !control) return false;

		// If we already have an active control, deactivate it first
		if (mActiveDragControl) {
			mActiveDragControl->Deactivate();
			mActiveDragControl = nullptr;
		}

		mActiveDragControl = control;
		if (mActiveDragControl->Init()) {
			Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "Activating drag control: %p", static_cast<void*>(static_cast<BaseDragViewInputControl*>(mActiveDragControl)));
			pView3D->SetCurrentViewInputControl(mActiveDragControl, cISC4View3DWin::ViewInputControlStackOperation_None);
			Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "Activated drag control: %p", static_cast<void*>(static_cast<BaseDragViewInputControl*>(mActiveDragControl)));
			return true;
		}

		mActiveDragControl = nullptr;
		return false;
	}


	void ShowMessageBox(const std::string& title, const std::string& message) {
		if (pWinMgr) {
			cRZBaseString titleStr(title);
			cRZBaseString messageStr(message);
			pWinMgr->GZMsgBox(messageStr, titleStr, 0, true, 0);
		}
		else {
			Logger::GetInstance().WriteLineFormatted(LogLevel::Error, "Failed to get window manager for message box: %s", message.c_str());
		}
	}

	std::vector<std::string> SplitString(const std::string& input) {
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

	void ProcessCheat(cIGZMessage2Standard* pStandardMsg) {
		const uint32_t cheatID = static_cast<uint32_t>(pStandardMsg->GetData1());

		if (cheatID == kTerrainExtensionsBridgeCheatID) {
			Logger::GetInstance().WriteLine(LogLevel::Info, "Bridge approach cheat code issued");
			ActivateBridgeDragMode();
			return;
		}
		if (cheatID == kTerrainExtensionsCheatID)
		{
			const cIGZString* pCheatString = static_cast<const cIGZString*>(pStandardMsg->GetVoid2());
			const std::string_view cheatStringView(pCheatString->Data(), pCheatString->Strlen());
			const size_t cheatStrignLength = cheatStringView.size();

			Logger& logger = Logger::GetInstance();
			logger.WriteLineFormatted(LogLevel::Info, "Cheat code issued: %s (ID: 0x%x)", cheatStringView.data(), cheatID);

			std::vector<std::string> tokens = SplitString(std::string(cheatStringView));

			logger.WriteLineFormatted(LogLevel::Info, "Parsing %d tokens:", static_cast<int>(tokens.size()));
			for (size_t i = 0; i < tokens.size(); ++i) {
				logger.WriteLineFormatted(LogLevel::Info, "  Token[%d]: '%s'", static_cast<int>(i), tokens[i].c_str());
			}

			std::vector<char*> argv;
			argv.reserve(tokens.size()); // Prevent reallocation

			for (size_t i = 0; i < tokens.size(); ++i) {
				argv.push_back(const_cast<char*>(tokens[i].c_str())); // Use c_str() for stability
			}

			args::ArgumentParser parser("Terrain extension tools");
				args::Group commands(parser, "commands");

				mToolRegistry.RegisterAllArguments(commands);
			args::HelpFlag help(parser, "help", "Show this help menu", { "help" });
			try {
				parser.ParseCLI(argv.size(), argv.data());

				if (!mToolRegistry.ExecuteAnyTool(parser)) {
					ShowMessageBox("Earthbender help", parser.Help());
					Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "No matching command found - showing help");
				}
			}
			catch (args::Help) {
				ShowMessageBox("Earthbender help", parser.Help());
				Logger::GetInstance().WriteLineFormatted(LogLevel::Info, "Help requested");
			}
			catch (args::ParseError e) {
				std::string errorMsg = "Command parse error: ";
				errorMsg += e.what();
				errorMsg += "\n\n";
				errorMsg += parser.Help();
				ShowMessageBox("Earthbender parse error", errorMsg);
				Logger::GetInstance().WriteLineFormatted(LogLevel::Error, "Parse error: %s", e.what());
			}
			catch (args::ValidationError e) {
				std::string errorMsg = "Command validation error: ";
				errorMsg += e.what();
				errorMsg += "\n\n";
				errorMsg += parser.Help();
				ShowMessageBox("Earthbender validation error", errorMsg);
				Logger::GetInstance().WriteLineFormatted(LogLevel::Error, "Validation error: %s", e.what());
			}
		}
	}

	bool PostAppInit()
	{
		cIGZMessageServer2Ptr pMS2;
		Logger& logger = Logger::GetInstance();
		logger.WriteLine(LogLevel::Info, "PostAppInit: Initializing TerrainExtensionsDllDirector");

		cIGZApp* const pApp = mpFrameWork->Application();

		if (pApp)
		{
			cRZAutoRefCount<cISC4App> sc4App;

			if (pApp->QueryInterface(GZIID_cISC4App, sc4App.AsPPVoid()))
			{
				pCheatCodeManager = sc4App->GetCheatCodeManager();
				pWinMgr = sc4App->GetMainWindow()->GetWindowManager();
			}
		}

		if (pMS2)
		{
			pMS2->AddNotification(this, kSC4MessagePostCityInit);
			pMS2->AddNotification(this, kSC4MessagePreCityShutdown);
			this->pMS2 = pMS2;
		}

		return true;
	}

	void PreCityShutdown(cIGZMessage2Standard* pStandardMsg)
	{
		cISC4View3DWin* localView3D = pView3D;
		pView3D = nullptr;

		if (localView3D)
		{
			localView3D->Release();
		}
	}
};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static TerrainExtensionsDllDirector sDirector;
	return &sDirector;
}