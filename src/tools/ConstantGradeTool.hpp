#pragma once

#include "TerrainTool.hpp"
#include "tools/constantgrade/ConstantGradeOperation.hpp"

#include <memory>

class ConstantGradeTool : public TerrainTool {
private:
	std::unique_ptr<args::Command> mCommand;
	std::unique_ptr<args::Positional<int>> mStartTileX, mStartTileZ, mEndTileX, mEndTileZ;
	std::unique_ptr<args::ValueFlag<float>> mWidthTiles, mGradePercent, mStartHeight, mEndHeight, mFalloffTiles;
	std::unique_ptr<args::ValueFlag<bool>> mAutoHeight;
	std::unique_ptr<args::Flag> mHardEdges;

public:
	explicit ConstantGradeTool(cISTETerrain* terrain);
	~ConstantGradeTool() override;

	void RegisterArguments(args::Group& commands) override;
	bool ShouldExecute(const args::ArgumentParser& parser) const override;
	void Execute(const args::ArgumentParser& parser) override;

	const char* GetName() const override;
	const char* GetDescription() const override;
	const char* GetUsage() const override;

private:
	void CreateConstantGradePath(
		int startTileX,
		int startTileZ,
		int endTileX,
		int endTileZ,
		float widthTiles,
		float gradePercent,
		float startH,
		float endH,
		bool autoHeight,
		float falloffTiles) const;
};
