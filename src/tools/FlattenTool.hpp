#pragma once

#include "TerrainTool.hpp"
#include "flatten/FlattenOperation.hpp"

#include <memory>
#include <string>

class FlattenTool : public TerrainTool {
private:
    std::unique_ptr<args::Command> mCommand;
    std::unique_ptr<args::Group> mRectGroup;
    std::unique_ptr<args::Positional<int>> mTileX1, mTileZ1, mTileX2, mTileZ2;
    std::unique_ptr<args::ValueFlag<float>> mHeight;
    std::unique_ptr<args::ValueFlag<std::string>> mMode;
    std::unique_ptr<args::ValueFlag<std::string>> mShape;
    std::unique_ptr<args::ValueFlag<int>> mThickness;

public:
    explicit FlattenTool(cISTETerrain* terrain);
    ~FlattenTool() override;

    void RegisterArguments(args::Group& commands) override;
    bool ShouldExecute(const args::ArgumentParser& parser) const override;
    void Execute(const args::ArgumentParser& parser) override;

    const char* GetName() const override;
    const char* GetDescription() const override;
    const char* GetUsage() const override;

private:
    static FlattenHeightMode ResolveMode_(const std::string& mode, bool hasHeight);
    static FlattenShapeMode ResolveShape_(const std::string& shape);
};
