#include "BridgeApproachCommand.hpp"
#include "utils/Logger.h"

BridgeApproachCommand::BridgeApproachCommand(cISTETerrain* terrain)
	: TerrainTool(terrain)
	  , mTool(terrain) {}

void BridgeApproachCommand::RegisterArguments(args::Group& commands) {
	mCommand = std::make_unique<args::Command>(
		commands, "bridgeapproach",
		"Create bridge approaches at both ends of a bridge span");

	mStartTileX = std::make_unique<args::Positional<int>>(
		*mCommand, "startx", "Bridge start tile X");
	mStartTileZ = std::make_unique<args::Positional<int>>(
		*mCommand, "startz", "Bridge start tile Z");
	mEndTileX = std::make_unique<args::Positional<int>>(
		*mCommand, "endx", "Bridge end tile X");
	mEndTileZ = std::make_unique<args::Positional<int>>(
		*mCommand, "endz", "Bridge end tile Z");

	mBridgeHeight = std::make_unique<args::ValueFlag<float>>(
		*mCommand, "height", "Bridge deck height in metres",
		args::Matcher{'h'});
	mApproachLength = std::make_unique<args::ValueFlag<float>>(
		*mCommand, "length", "Approach length in tiles (-1 = auto)",
		args::Matcher{'l'}, -1.0f);
	mMaxGrade = std::make_unique<args::ValueFlag<float>>(
		*mCommand, "grade", "Maximum grade percentage",
		args::Matcher{'g'}, 6.0f);
	mWidthTiles = std::make_unique<args::ValueFlag<float>>(
		*mCommand, "width", "Approach width in tiles",
		args::Matcher{'w'}, 2.0f);
	mTaper = std::make_unique<args::Flag>(
		*mCommand, "taper", "Enable width tapering",
		args::Matcher{"taper"});
}

bool BridgeApproachCommand::ShouldExecute(
	const args::ArgumentParser& parser) const {
	return mCommand && *mCommand && mBridgeHeight && *mBridgeHeight;
}

void BridgeApproachCommand::Execute(const args::ArgumentParser& parser) {
	const int startX = args::get(*mStartTileX);
	const int startZ = args::get(*mStartTileZ);
	const int endX = args::get(*mEndTileX);
	const int endZ = args::get(*mEndTileZ);
	const float height = args::get(*mBridgeHeight);
	const float length = args::get(*mApproachLength);
	const float grade = args::get(*mMaxGrade);
	const float width = args::get(*mWidthTiles);
	const bool taper = mTaper && *mTaper;

	LOG_INFO("BridgeApproachCommand: ({},{}) -> ({},{}) "
	         "height={:.2f} grade={:.1f}% width={:.1f} taper={}",
	         startX, startZ, endX, endZ, height, grade, width, taper);

	mTool.CreateBridgeApproaches(
		startX, startZ, endX, endZ,
		height, length, grade, width, taper);
}

const char* BridgeApproachCommand::GetName() const {
	return "bridgeapproach";
}

const char* BridgeApproachCommand::GetDescription() const {
	return "Create bridge approaches at both ends of a bridge span";
}

const char* BridgeApproachCommand::GetUsage() const {
	return "bridgeapproach <startx> <startz> <endx> <endz> "
		"--height=<m> [--length=<tiles>] [--grade=<%>] "
		"[--width=<tiles>] [--taper]";
}
