#pragma once
#include "TerrainTool.hpp"
#include "BlueprintData.hpp"
#include "cISC4City.h"
#include "cISC4ZoneManager.h"
#include "cISC4OccupantManager.h"
#include "cISC4NetworkOccupant.h"
#include "cISC4Occupant.h"
#include "cISC4City.h"
#include "SC4List.h"
#include <unordered_set>
#include "cRZAutoRefCount.h"
#include <vector>
#include <unordered_map>
#include <sstream>
#include "filters/NetworkOccupantFilter.h"

namespace {
	struct NetworkIterData {
		int x1; int z1; int x2; int z2;
		std::unordered_set<cISC4Occupant*>* visited;
		CapturedBlueprint* bp;
	};

	static bool CollectNetworkCallback(cISC4Occupant* occ, void* pData) {
		if (!occ || !pData) return true; // continue
		auto* data = reinterpret_cast<NetworkIterData*>(pData);
		if (data->visited->contains(occ)) return true;
		data->visited->insert(occ);
		cRZAutoRefCount<cISC4NetworkOccupant> netOcc;
		if (!occ->QueryInterface(GZIID_cISC4NetworkOccupant, netOcc.AsPPVoid())) return true;
		uint32_t cellX=0, cellZ=0; netOcc->GetOccupiedCell(cellX, cellZ);
		if (cellX < static_cast<uint32_t>(data->x1) || cellX > static_cast<uint32_t>(data->x2) || cellZ < static_cast<uint32_t>(data->z1) || cellZ > static_cast<uint32_t>(data->z2)) return true;
		CapturedNetworkPiece piece;
		piece.relX = static_cast<int>(cellX) - data->x1;
		piece.relZ = static_cast<int>(cellZ) - data->z1;
		piece.pieceId = netOcc->PieceId();
		piece.rotation = netOcc->GetRotation();
		piece.flip = netOcc->GetFlip();
		piece.variation = netOcc->GetVariation();
		for (uint32_t t = 0; t <= 12; ++t) {
			if (netOcc->IsOfType(static_cast<cISC4NetworkOccupant::eNetworkType>(t))) { piece.networkType = t; break; }
		}
		piece.isIntersection = netOcc->IsIntersection();
		data->bp->networkPieces.push_back(piece);
		return true; // continue
	}

	static const char* ZoneTypeToString(int32_t v) {
		// Mirrors cISC4ZoneManager::ZoneType integral values
		switch (v) {
			case 0: return "None";
			case 1: return "ResidentialLowDensity";
			case 2: return "ResidentialMediumDensity";
			case 3: return "ResidentialHighDensity";
			case 4: return "CommercialLowDensity";
			case 5: return "CommercialMediumDensity";
			case 6: return "CommercialHighDensity";
			case 7: return "Agriculture";
			case 8: return "IndustrialMediumDensity";
			case 9: return "IndustrialHighDensity";
			case 10: return "Military";
			case 11: return "Airport";
			case 12: return "Seaport";
			case 13: return "Spaceport";
			case 14: return "Landfill";
			case 15: return "Plopped";
			default: return "Unknown";
		}
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

		CapturedBlueprint bp; // use shared struct
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

		cISC4OccupantManager* occMgr = mCity->GetOccupantManager();
		if (occMgr) {
			int debugVisitedCells = 0;
			int debugFoundOcc = 0;
			int debugDuplicates = 0;
			// Reuse a single filter instance instead of allocating each call
			auto* netFilter = new NetworkOccupantFilter(NetworkTypeFlags::AllTransportationNetworks);
			std::unordered_set<cISC4Occupant*> seenNetworkOccupants;
			for (int cz = z1; cz <= z2; ++cz) {
				for (int cx = x1; cx <= x2; ++cx) {
					++debugVisitedCells;
					cISC4Occupant* occ = nullptr;
					if (occMgr->GetFirstOccupantByStandardCityCell(occ, cx, cz, netFilter) && occ) {
						++debugFoundOcc;
						cRZAutoRefCount<cISC4NetworkOccupant> netOcc;
						if (occ->QueryInterface(GZIID_cISC4NetworkOccupant, netOcc.AsPPVoid())) {
							// Avoid duplicate recording if same occupant spans multiple queried cells
							if (!seenNetworkOccupants.insert(occ).second) { ++debugDuplicates; continue; }
							uint32_t cellX=0, cellZ=0; netOcc->GetOccupiedCell(cellX, cellZ);
							CapturedNetworkPiece piece;
							piece.relX = static_cast<int>(cellX) - x1;
							piece.relZ = static_cast<int>(cellZ) - z1;
							piece.pieceId = netOcc->PieceId();
							piece.rotation = netOcc->GetRotation();
							piece.flip = netOcc->GetFlip();
							piece.variation = netOcc->GetVariation();
							for (uint32_t t=0; t<=12; ++t) {
								if (netOcc->IsOfType(static_cast<cISC4NetworkOccupant::eNetworkType>(t))) { piece.networkType = t; break; }
							}
							piece.isIntersection = netOcc->IsIntersection();
							bp.networkPieces.push_back(piece);
						}
					}
				}
			}
			delete netFilter;
			LOG_INFO("BlueprintCapture: scanned cells={} first-occupants={} networks={} duplicatesSkipped={}", debugVisitedCells, debugFoundOcc, bp.networkPieces.size(), debugDuplicates);
		} else {
			LOG_INFO("BlueprintCapture: OccupantManager unavailable, skipped network capture");
		}

		// Commit global blueprint
		g_LastCapturedBlueprint = std::move(bp);

		// Summary log
		std::ostringstream oss;
		oss << "Zones summary:";
		for (const auto& kv : g_LastCapturedBlueprint.zoneCounts) {
			auto enumVal = static_cast<cISC4ZoneManager::ZoneType>(kv.first);
			oss << ' ' << ZoneTypeToString(static_cast<int>(enumVal)) << '=' << kv.second;
		}
		int nonEmpty = 0;
		for (int v : g_LastCapturedBlueprint.zoneTypes) if (v != 0) ++nonEmpty;
		LOG_INFO("BlueprintCapture: {}", oss.str().c_str());
		LOG_INFO("BlueprintCapture: total cells={} non-empty={} networks={}",
			g_LastCapturedBlueprint.width * g_LastCapturedBlueprint.height,
			nonEmpty,
			g_LastCapturedBlueprint.networkPieces.size());
	}

	const char* GetName() const override { return "blueprint-capture"; }
	const char* GetDescription() const override { return "Capture zoning + networks into an internal blueprint"; }
	const char* GetUsage() const override { return "blueprint-capture <x1> <z1> <x2> <z2>"; }
};
