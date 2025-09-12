#pragma once
#include "TerrainTool.hpp"
#include "BlueprintData.hpp"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <memory>

using nlohmann::json;

static const char* ZoneTypeToStringExport(int32_t v) {
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

class BlueprintExportTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::ValueFlag<std::string>> mFile;
	std::unique_ptr<args::ValueFlag<int>> mIndent;
	std::unique_ptr<args::Flag> mPretty;
public:
	explicit BlueprintExportTool(cISTETerrain* terrain) : TerrainTool(terrain) {}

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, GetName(), GetDescription());
		mFile = std::make_unique<args::ValueFlag<std::string>>(*mCommand, "file", "Output JSON file path", args::Matcher{"file","f"});
		mIndent = std::make_unique<args::ValueFlag<int>>(*mCommand, "indent", "Pretty-print indent (implies --pretty)", args::Matcher{"indent"}, 2);
		mPretty = std::make_unique<args::Flag>(*mCommand, "pretty", "Pretty-print JSON", args::Matcher{"pretty","p"});
	}

	bool ShouldExecute(const args::ArgumentParser&) const override { return mCommand && *mCommand; }

	void Execute(const args::ArgumentParser&) override {
		if (!g_LastCapturedBlueprint.IsValid()) {
			LOG_ERROR("BlueprintExport: no valid blueprint captured yet");
			return;
		}

		std::string path;
		if (*mFile) {
			path = args::get(*mFile);
		} else {
			// Default: current working directory
			path = "blueprint_capture.json";
		}

		bool pretty = *mPretty || *mIndent;
		int indent = pretty ? args::get(*mIndent) : -1;
		if (indent < 0) indent = 2;

		const auto& bp = g_LastCapturedBlueprint;

		json j;
		j["origin"] = { {"x", bp.originX}, {"z", bp.originZ} };
		j["size"] = { {"width", bp.width}, {"height", bp.height} };

		// Zone counts
		json zCounts = json::object();
		for (const auto& kv : bp.zoneCounts) {
			zCounts[ZoneTypeToStringExport(kv.first)] = kv.second;
		}
		// Zone cells: store as flattened array (int enum values)
		json zCells = json::array();
		for (int32_t v : bp.zoneTypes) zCells.push_back(v);

		int nonEmpty = 0; for (int32_t v : bp.zoneTypes) if (v != 0) ++nonEmpty;

		j["zones"] = {
			{"encoding", "cISC4ZoneManager::ZoneType"},
			{"counts", zCounts},
			{"cellsFlat", zCells},
			{"nonEmpty", nonEmpty}
		};

		// Networks
		json nets = json::array();
		for (const auto& np : bp.networkPieces) {
			nets.push_back({
				{"relX", np.relX},
				{"relZ", np.relZ},
				{"networkType", np.networkType},
				{"pieceId", np.pieceId},
				{"rotation", np.rotation},
				{"rotationAndFlip", np.rotationAndFlip},
				{"flip", np.flip},
				{"variation", np.variation},
				{"isIntersection", np.isIntersection}
			});
		}
		j["networks"] = nets;

		// Parcels
		json parcels = json::array();
		for (const auto& p : bp.zoneLotParcels) {
			parcels.push_back({
{"zoneType", p.zoneType},
				{"relX", p.relX},
				{"relZ", p.relZ},
				{"width", p.width},
				{"height", p.height},
				{"facing", p.facing},
				{"hasBuilding", p.hasBuilding},
				{"isHistorical", p.isHistorical},
				{"habitationState", p.habitationState}
			});
		}
		j["parcels"] = parcels;

		j["summary"] = {
			{"totalCells", bp.width * bp.height},
			{"zoneNonEmpty", nonEmpty},
			{"networkCount", bp.networkPieces.size()},
			{"parcelCount", bp.zoneLotParcels.size()}
		};

		std::error_code ec;
		std::filesystem::path p(path);
		auto parent = p.parent_path();
		if (!parent.empty()) {
			std::error_code ec2;
			std::filesystem::create_directories(parent, ec2);
			if (ec2) {
				LOG_ERROR("BlueprintExport: failed to create directory '{}': {}", parent.string(), ec2.message());
			}
		} else {
			LOG_DEBUG("BlueprintExport: no parent directory specified, writing file to current working directory");
		}

		std::ofstream ofs(path, std::ios::trunc | std::ios::out | std::ios::binary);
		if (!ofs) {
			LOG_ERROR("BlueprintExport: failed to open output file: {}", path);
			return;
		}

		if (pretty) {
			ofs << j.dump(indent) << '\n';
		} else {
			ofs << j.dump() << '\n';
		}
		ofs.close();

		if (!ofs.fail()) {
			LOG_INFO("BlueprintExport: wrote blueprint to {} (zones={}, networks={})", path, bp.zoneTypes.size(), bp.networkPieces.size());
		} else {
			LOG_ERROR("BlueprintExport: I/O error writing {}", path);
		}
	}

	const char* GetName() const override { return "blueprint-export"; }
	const char* GetDescription() const override { return "Export last captured blueprint to JSON"; }
	const char* GetUsage() const override { return "blueprint-export [--file=<path>] [--pretty] [--indent=<n>]"; }
};
