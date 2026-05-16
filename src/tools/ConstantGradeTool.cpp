#include "ConstantGradeTool.hpp"

#include "utils/Logger.h"

#include <optional>

ConstantGradeTool::ConstantGradeTool(cISTETerrain* terrain)
	: TerrainTool(terrain) {
}

ConstantGradeTool::~ConstantGradeTool() = default;

void ConstantGradeTool::RegisterArguments(args::Group& commands) {
	mCommand = std::make_unique<args::Command>(commands, "constantgrade",
		"Create path with constant grade");

	mStartTileX = std::make_unique<args::Positional<int>>(*mCommand, "startx", "Start tile X");
	mStartTileZ = std::make_unique<args::Positional<int>>(*mCommand, "startz", "Start tile Z");
	mEndTileX = std::make_unique<args::Positional<int>>(*mCommand, "endx", "End tile X");
	mEndTileZ = std::make_unique<args::Positional<int>>(*mCommand, "endz", "End tile Z");

	mWidthTiles = std::make_unique<args::ValueFlag<float>>(*mCommand, "width",
		"Path width in tiles",
		args::Matcher{ 'w' }, 1.0f);
	mGradePercent = std::make_unique<args::ValueFlag<float>>(*mCommand, "grade",
		"Target grade percentage",
		args::Matcher{ 'g' }, -1000.0f);
	mStartHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "start",
		"Start height",
		args::Matcher{ 's' }, -1000.0f);
	mEndHeight = std::make_unique<args::ValueFlag<float>>(*mCommand, "end",
		"End height",
		args::Matcher{ 'e' }, -1000.0f);
	mAutoHeight = std::make_unique<args::ValueFlag<bool>>(*mCommand, "auto",
		"Auto-detect heights",
		args::Matcher{ 'a' }, false);
	mHardEdges = std::make_unique<args::Flag>(*mCommand, "hard-edges",
		"Disable side smoothing across the tool width",
		args::Matcher{"hard-edges"});
}

bool ConstantGradeTool::ShouldExecute(const args::ArgumentParser& parser) const {
	return *mCommand;
}

void ConstantGradeTool::Execute(const args::ArgumentParser& parser) {
	int startTileX = args::get(*mStartTileX);
	int startTileZ = args::get(*mStartTileZ);
	int endTileX = args::get(*mEndTileX);
	int endTileZ = args::get(*mEndTileZ);
	float widthTiles = args::get(*mWidthTiles);
	float gradePercent = args::get(*mGradePercent);
	float startHeight = args::get(*mStartHeight);
	float endHeight = args::get(*mEndHeight);
	bool autoHeight = args::get(*mAutoHeight);
	const bool sideSmoothing = !static_cast<bool>(*mHardEdges);

	LOG_INFO("Creating constant grade path from tile ({},{}) to ({},{}), width: {:.1f} tiles",
		startTileX, startTileZ, endTileX, endTileZ, widthTiles);

	CreateConstantGradePath(startTileX, startTileZ, endTileX, endTileZ,
		widthTiles, gradePercent, startHeight, endHeight, autoHeight, sideSmoothing);
}

const char* ConstantGradeTool::GetName() const {
	return "constantgrade";
}

const char* ConstantGradeTool::GetDescription() const {
	return "Create path with constant grade";
}

const char* ConstantGradeTool::GetUsage() const {
	return "constantgrade <startx> <startz> <endx> <endz> [--width=<tiles>] [--grade=<percent>] "
		"[--start=<height>] [--end=<height>] [--auto] [--hard-edges]";
}

void ConstantGradeTool::CreateConstantGradePath(
	int startTileX,
	int startTileZ,
	int endTileX,
	int endTileZ,
	float widthTiles,
	float gradePercent,
	float startH,
	float endH,
	bool autoHeight,
	bool sideSmoothing) const {
	ConstantGradeOperation operation(terrain_);
	const ConstantGradeRequest request{
		.startTileX = startTileX,
		.startTileZ = startTileZ,
		.endTileX = endTileX,
		.endTileZ = endTileZ,
		.widthTiles = widthTiles,
		.gradePercent = gradePercent > -999.0f ? std::optional<float>(gradePercent) : std::nullopt,
		.startHeight = (!autoHeight && startH > -999.0f) ? std::optional<float>(startH) : std::nullopt,
		.endHeight = (!autoHeight && gradePercent <= -999.0f && endH > -999.0f)
			? std::optional<float>(endH)
			: std::nullopt,
		.sideSmoothing = sideSmoothing,
	};

	if (!operation.Apply(request)) {
		LOG_WARN("Constant grade path creation skipped because the request was invalid");
	}
}
