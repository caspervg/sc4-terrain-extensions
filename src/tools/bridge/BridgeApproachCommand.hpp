#pragma once
#include "BridgeApproachTool.hpp"
#include "tools/TerrainTool.hpp"


class BridgeApproachCommand : public TerrainTool {
public:
    explicit BridgeApproachCommand(cISTETerrain* terrain);
    ~BridgeApproachCommand() override = default;

    void RegisterArguments(args::Group& commands) override;
    bool ShouldExecute(const args::ArgumentParser& parser) const override;
    void Execute(const args::ArgumentParser& parser) override;

    const char* GetName() const override;
    const char* GetDescription() const override;
    const char* GetUsage() const override;

private:
    BridgeApproachTool mTool;

    std::unique_ptr<args::Command> mCommand;
    std::unique_ptr<args::Positional<int>> mStartTileX;
    std::unique_ptr<args::Positional<int>> mStartTileZ;
    std::unique_ptr<args::Positional<int>> mEndTileX;
    std::unique_ptr<args::Positional<int>> mEndTileZ;
    std::unique_ptr<args::ValueFlag<float>> mBridgeHeight;
    std::unique_ptr<args::ValueFlag<float>> mApproachLength;
    std::unique_ptr<args::ValueFlag<float>> mMaxGrade;
    std::unique_ptr<args::ValueFlag<float>> mWidthTiles;
    std::unique_ptr<args::Flag> mTaper;
};
