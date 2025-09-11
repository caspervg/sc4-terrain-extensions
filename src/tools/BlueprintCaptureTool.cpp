#pragma once
#include "TerrainTool.hpp"
#include "cISC4City.h"
#include "cISC4ZoneManager.h"
#include "cISC4OccupantManager.h"
#include "cISC4Occupant.h"
#include "cISC4NetworkOccupant.h"
#include "utils/Logger.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>

// Phase 1: Capture zoning (and stub for network occupants) inside a rectangle.
// Stores the latest captured blueprint in static memory for later stamping phases.

struct CapturedBlueprintPhase1 {
	int originX = 0;
	int originZ = 0;
	int width = 0;   // inclusive span (#cells in X)
	int height = 0;  // inclusive span (#cells in Z)
	// Zone type map (same ordering as scan: z-major or row-major). We'll use row-major: index = (z*width + x)
	std::vector<int32_t> zoneTypes; // store underlying enum integral values
	// Simple counts per zone type value
	std::unordered_map<int32_t, int> zoneCounts;
};

static CapturedBlueprintPhase1 g_LastCapturedBlueprint; // global storage (simple for now)

static const char* ZoneTypeToString(cISC4ZoneManager::ZoneType t) {
	using Z = cISC4ZoneManager::ZoneType;
	switch (t) {
	case Z::None: return "None";
	case Z::ResidentialLowDensity: return "ResLow";
	case Z::ResidentialMediumDensity: return "ResMed";
	case Z::ResidentialHighDensity: return "ResHigh";
	case Z::CommercialLowDensity: return "ComLow";
	case Z::CommercialMediumDensity: return "ComMed";
	case Z::CommercialHighDensity: return "ComHigh";
	case Z::Agriculture: return "Agri";
	case Z::IndustrialMediumDensity: return "IndMed";
	case Z::IndustrialHighDensity: return "IndHigh";
	case Z::Military: return "Military";
	case Z::Airport: return "Airport";
	case Z::Seaport: return "Seaport";
	case Z::Spaceport: return "Spaceport";
	case Z::Landfill: return "Landfill";
	case Z::Plopped: return "Plopped";
	default: return "Unknown";
	}
}

class BlueprintCaptureTool : public TerrainTool {
private:
	cISC4City* mCity; // non-owning
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::Positional<int>> mX1, mZ1, mX2, mZ2;

public:
	BlueprintCaptureTool(cISTETerrain* terrain, cISC4City* city)
		: TerrainTool(terrain), mCity(city) {}

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, GetName(), GetDescription());
		mX1 = std::make_unique<args::Positional<int>>(*mCommand, "x1", "Start tile X");
		mZ1 = std::make_unique<args::Positional<int>>(*mCommand, "z1", "Start tile Z");
		mX2 = std::make_unique<args::Positional<int>>(*mCommand, "x2", "End tile X");
		mZ2 = std::make_unique<args::Positional<int>>(*mCommand, "z2", "End tile Z");
	}

	bool ShouldExecute(const args::ArgumentParser& parser) const override {
		return mCommand && *mCommand; // command invoked
	}

	void Execute(const args::ArgumentParser& parser) override {
		if (!mCity) {
			LOG_ERROR("BlueprintCaptureTool: City pointer is null");
			return;
		}
		cISC4ZoneManager* zoneMgr = mCity->GetZoneManager();
		if (!zoneMgr) {
			LOG_ERROR("BlueprintCaptureTool: ZoneManager is null");
			return;
		}

		int x1 = args::get(*mX1);
		int z1 = args::get(*mZ1);
		int x2 = args::get(*mX2);
		int z2 = args::get(*mZ2);

		// Normalize rectangle ordering
		if (x2 < x1) std::swap(x1, x2);
		if (z2 < z1) std::swap(z1, z2);

		// Clamp to terrain bounds
		ClampToTerrainBounds(x1, z1);
		ClampToTerrainBounds(x2, z2);

		int width = x2 - x1 + 1;
		int height = z2 - z1 + 1;
		if (width <= 0 || height <= 0) {
			LOG_ERROR("BlueprintCaptureTool: Invalid rectangle after normalization");
			return;
		}

		CapturedBlueprintPhase1 bp;
		bp.originX = x1;
		bp.originZ = z1;
		bp.width = width;
		bp.height = height;
		bp.zoneTypes.resize(static_cast<size_t>(width * height));
		bp.zoneCounts.clear();

		LOG_INFO("BlueprintCapture: capturing rectangle ({} , {}) to ({} , {}) size {}x{}", x1, z1, x2, z2, width, height);

		// Capture zones
		for (int dz = 0; dz < height; ++dz) {
			for (int dx = 0; dx < width; ++dx) {
				int worldX = x1 + dx;
				int worldZ = z1 + dz;
				cISC4ZoneManager::ZoneType zt = zoneMgr->GetZoneType(worldX, worldZ);
				int idx = dz * width + dx;
				bp.zoneTypes[idx] = static_cast<int32_t>(zt);
				bp.zoneCounts[static_cast<int32_t>(zt)]++;
			}
		}

		// (Phase 1) Network occupant capture stub: logged only; full metadata capture will come in Phase 2
		LOG_INFO("BlueprintCapture: network occupant capture deferred (phase 1 stub)");

		// Commit global
		g_LastCapturedBlueprint = std::move(bp);

		// Summary log
		std::ostringstream oss;
		oss << "Zones summary:";
		for (const auto& kv : g_LastCapturedBlueprint.zoneCounts) {
			auto enumVal = static_cast<cISC4ZoneManager::ZoneType>(kv.first);
			oss << ' ' << ZoneTypeToString(enumVal) << '=' << kv.second;
		}
		LOG_INFO("BlueprintCapture: {}", oss.str().c_str());
		int nonEmpty = 0;
		for (int v : g_LastCapturedBlueprint.zoneTypes) {
			if (v != 0) ++nonEmpty;
		}
		LOG_INFO("BlueprintCapture: total cells={} non-empty={} (excluding None)",
			g_LastCapturedBlueprint.width * g_LastCapturedBlueprint.height,
			nonEmpty);
	}

	const char* GetName() const override { return "blueprint-capture"; }
	const char* GetDescription() const override { return "Capture zoning (phase 1) into an internal blueprint"; }
	const char* GetUsage() const override { return "blueprint-capture <x1> <z1> <x2> <z2>"; }
};
