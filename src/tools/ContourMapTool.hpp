#pragma once

#include <memory>

#include "TerrainTool.hpp"

class TerrainContourRenderer;

class ContourMapTool : public TerrainTool {
public:
    ContourMapTool(cISTETerrain* terrain, TerrainContourRenderer& renderer);
    ~ContourMapTool() override = default;

    void RegisterArguments(args::Group& commands) override;
    bool ShouldExecute(const args::ArgumentParser& parser) const override;
    void Execute(const args::ArgumentParser& parser) override;

    const char* GetName() const override;
    const char* GetDescription() const override;
    const char* GetUsage() const override;

private:
    TerrainContourRenderer& renderer_;

    std::unique_ptr<args::Command> mCommand;
    std::unique_ptr<args::Flag> mOn;
    std::unique_ptr<args::Flag> mOff;
    std::unique_ptr<args::Flag> mToggle;
    std::unique_ptr<args::ValueFlag<float>> mIntervalMeters;
    std::unique_ptr<args::ValueFlag<int>> mMajorEvery;
    std::unique_ptr<args::Flag> mRefresh;
};
