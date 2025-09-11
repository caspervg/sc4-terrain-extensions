#pragma once
#include "TerrainTool.hpp"
#include <algorithm>

class FlattenTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::Group> mRectGroup;
	std::unique_ptr<args::Positional<int>> mTileX1, mTileZ1, mTileX2, mTileZ2;
	std::unique_ptr<args::ValueFlag<float>> mHeight;
	std::unique_ptr<args::ValueFlag<std::string>> mMode;

public:
	FlattenTool(cISTETerrain* terrain) : TerrainTool(terrain) {
	}

	~FlattenTool() override = default;

	void RegisterArguments(args::Group& commands) override {
		mCommand = std::make_unique<args::Command>(commands, "flatten",
			"Flatten terrain to specified height");

		mTileX1 = std::make_unique<args::Positional<int>>(*mCommand, "x1", "Tile X coordinate");
		mTileZ1 = std::make_unique<args::Positional<int>>(*mCommand, "z1", "Tile Z coordinate");
		
		mRectGroup = std::make_unique<args::Group>(*mCommand, "Rectangle coordinates (optional, both or neither):", args::Group::Validators::AllOrNone);
		mTileX2 = std::make_unique<args::Positional<int>>(*mRectGroup, "x2", "End tile X");
		mTileZ2 = std::make_unique<args::Positional<int>>(*mRectGroup, "z2", "End tile Z");

		mHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "height",
			"Target height", args::Matcher{ "height", 'h' });
		mMode = std::make_unique<args::ValueFlag<std::string>>(*mCommand, "mode",
			"Height calculation mode (avg|min|max)", args::Matcher{ "mode", 'm'}, "avg");
	}

	bool ShouldExecute(const args::ArgumentParser& parser) const override {
		return *mCommand;
	}

	void Execute(const args::ArgumentParser& parser) override {
		int tileX1 = args::get(*mTileX1);
		int tileZ1 = args::get(*mTileZ1);
		
		bool isRectangleMode = *mTileX2 && *mTileZ2;
		int tileX2 = isRectangleMode ? args::get(*mTileX2) : tileX1;
		int tileZ2 = isRectangleMode ? args::get(*mTileZ2) : tileZ1;
		
		bool hasHeight = *mHeight;
		float targetHeight = hasHeight ? args::get(*mHeight) : 0.0f;
		std::string mode = args::get(*mMode);

		if (isRectangleMode) {
			LOG_INFO("Flattening rectangle from ({},{}) to ({},{})", tileX1, tileZ1, tileX2, tileZ2);
		} else {
			LOG_INFO("Flattening single tile ({},{})", tileX1, tileZ1);
		}

		FlattenArea(tileX1, tileZ1, tileX2, tileZ2, targetHeight, mode, isRectangleMode, hasHeight);
	}

	const char* GetName() const override {
		return "flatten";
	}

	const char* GetDescription() const override {
		return "Flatten terrain to specified height";
	}

	const char* GetUsage() const override {
		return "flatten <x1> <z1> [<x2> <z2>] --height=<value> [--mode=<avg|min|max>]";
	}

private:
	void FlattenArea(int x1, int z1, int x2, int z2, float specifiedHeight, const std::string& mode, bool isRectangle, bool hasHeight) {
		// Ensure proper bounds
		int minX = std::min(x1, x2);
		int maxX = std::max(x1, x2);
		int minZ = std::min(z1, z2);
		int maxZ = std::max(z1, z2);

		// Calculate target height
		float targetHeight = CalculateTargetHeight(minX, minZ, maxX, maxZ, specifiedHeight, mode, isRectangle, hasHeight);
		
		LOG_DEBUG("Target height: {:.2f}", targetHeight);

		// Flatten all vertices in the area
		for (int z = minZ; z <= maxZ + 1; z++) {
			for (int x = minX; x <= maxX + 1; x++) {
				SetAltitudeAtVertex(x, z, targetHeight);
			}
		}

		// Refresh terrain display
		SC4Rect<int32_t> refreshRect(minX, minZ, maxX + 1, maxZ + 1);
		Refresh(refreshRect);
		
		LOG_DEBUG("Terrain flattening completed");
	}

	float CalculateTargetHeight(int minX, int minZ, int maxX, int maxZ, float specifiedHeight, const std::string& mode, bool isRectangle, bool hasHeight) {
		// If height is specified, use it
		if (hasHeight) {
			return specifiedHeight;
		}

		// For single tile mode, height is required
		if (!isRectangle) {
			LOG_DEBUG("Height parameter required for single tile mode");
			return GetTileAverageHeight(minX, minZ); // Fallback to current height
		}

		// Calculate height based on mode for rectangle
		if (mode == "min") {
			return FindMinHeight(minX, minZ, maxX, maxZ);
		} else if (mode == "max") {
			return FindMaxHeight(minX, minZ, maxX, maxZ);
		} else {
			return CalculateAverageHeight(minX, minZ, maxX, maxZ);
		}
	}

	float FindMinHeight(int minX, int minZ, int maxX, int maxZ) {
		float minHeight = std::numeric_limits<float>::max();
		
		for (int z = minZ; z <= maxZ + 1; z++) {
			for (int x = minX; x <= maxX + 1; x++) {
				if (mTerrain->LocationIsInBounds(static_cast<float>(x), static_cast<float>(z))) {
					float height = mTerrain->GetAltitudeAtVertex(x, z);
					minHeight = std::min(minHeight, height);
				}
			}
		}
		return minHeight;
	}

	float FindMaxHeight(int minX, int minZ, int maxX, int maxZ) {
		float maxHeight = std::numeric_limits<float>::lowest();
		
		for (int z = minZ; z <= maxZ + 1; z++) {
			for (int x = minX; x <= maxX + 1; x++) {
				if (mTerrain->LocationIsInBounds(static_cast<float>(x), static_cast<float>(z))) {
					float height = mTerrain->GetAltitudeAtVertex(x, z);
					maxHeight = std::max(maxHeight, height);
				}
			}
		}
		return maxHeight;
	}

	float CalculateAverageHeight(int minX, int minZ, int maxX, int maxZ) {
		float totalHeight = 0.0f;
		int count = 0;
		
		for (int z = minZ; z <= maxZ + 1; z++) {
			for (int x = minX; x <= maxX + 1; x++) {
				if (mTerrain->LocationIsInBounds(static_cast<float>(x), static_cast<float>(z))) {
					totalHeight += mTerrain->GetAltitudeAtVertex(x, z);
					count++;
				}
			}
		}
		return count > 0 ? totalHeight / count : 0.0f;
	}
};