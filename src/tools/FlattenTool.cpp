#pragma once
#include "TerrainTool.hpp"
#include <algorithm>

#include "flatten/FlattenOperation.hpp"

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
		const int tileX1 = args::get(*mTileX1);
		const int tileZ1 = args::get(*mTileZ1);
		const bool isRectangleMode = *mTileX2 && *mTileZ2;
		const int tileX2 = isRectangleMode ? args::get(*mTileX2) : tileX1;
		const int tileZ2 = isRectangleMode ? args::get(*mTileZ2) : tileZ1;
		const bool hasHeight = *mHeight;
		const float targetHeight = hasHeight ? args::get(*mHeight) : 0.0f;
		const std::string mode = args::get(*mMode);

		if (isRectangleMode) {
			LOG_INFO("Flattening rectangle from ({},{}) to ({},{})", tileX1, tileZ1, tileX2, tileZ2);
		} else {
			LOG_INFO("Flattening single tile ({},{})", tileX1, tileZ1);
		}

		FlattenOperation operation(terrain_);
		operation.Apply(FlattenRequest{
			.x1 = tileX1,
			.z1 = tileZ1,
			.x2 = tileX2,
			.z2 = tileZ2,
			.mode = ResolveMode_(mode, hasHeight),
			.explicitHeight = targetHeight
		});
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
	static FlattenHeightMode ResolveMode_(const std::string& mode, const bool hasHeight) {
		if (hasHeight) {
			return FlattenHeightMode::Explicit;
		}
		if (mode == "min") {
			return FlattenHeightMode::Minimum;
		}
		if (mode == "max") {
			return FlattenHeightMode::Maximum;
		}
		if (mode == "delta") {
			return FlattenHeightMode::Delta;
		}
		return FlattenHeightMode::Average;
	}
};
