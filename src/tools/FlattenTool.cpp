#include "FlattenTool.hpp"

#include "utils/Logger.h"

FlattenTool::FlattenTool(cISTETerrain* terrain)
    : TerrainTool(terrain) {
}

FlattenTool::~FlattenTool() = default;

void FlattenTool::RegisterArguments(args::Group& commands) {
    mCommand = std::make_unique<args::Command>(commands, "flatten",
        "Flatten terrain to specified height");

    mTileX1 = std::make_unique<args::Positional<int>>(*mCommand, "x1", "Tile X coordinate");
    mTileZ1 = std::make_unique<args::Positional<int>>(*mCommand, "z1", "Tile Z coordinate");

    mRectGroup = std::make_unique<args::Group>(
        *mCommand,
        "Rectangle coordinates (optional, both or neither):",
        args::Group::Validators::AllOrNone);
    mTileX2 = std::make_unique<args::Positional<int>>(*mRectGroup, "x2", "End tile X");
    mTileZ2 = std::make_unique<args::Positional<int>>(*mRectGroup, "z2", "End tile Z");

    mHeight = std::make_unique<args::ValueFlag<float>>(
        *mCommand,
        "height",
        "Target height or delta height when --mode=delta",
        args::Matcher{"height", 'h'});
    mMode = std::make_unique<args::ValueFlag<std::string>>(
        *mCommand,
        "mode",
        "Height calculation mode (avg|refavg|min|max|delta)",
        args::Matcher{"mode", 'm'},
        "avg");
    mShape = std::make_unique<args::ValueFlag<std::string>>(
        *mCommand,
        "shape",
        "Selection shape (rect|line)",
        args::Matcher{"shape", 's'},
        "rect");
    mThickness = std::make_unique<args::ValueFlag<int>>(
        *mCommand,
        "thickness",
        "Line-mask thickness (-9..-1 or 1..9)",
        args::Matcher{"thickness", 't'});
}

bool FlattenTool::ShouldExecute(const args::ArgumentParser& parser) const {
    return *mCommand;
}

void FlattenTool::Execute(const args::ArgumentParser& parser) {
    const int tileX1 = args::get(*mTileX1);
    const int tileZ1 = args::get(*mTileZ1);
    const bool isRectangleMode = *mTileX2 && *mTileZ2;
    const int tileX2 = isRectangleMode ? args::get(*mTileX2) : tileX1;
    const int tileZ2 = isRectangleMode ? args::get(*mTileZ2) : tileZ1;
    const bool hasHeight = *mHeight;
    const float heightValue = hasHeight ? args::get(*mHeight) : 0.0f;
    const std::string mode = args::get(*mMode);
    const std::string shape = args::get(*mShape);
    const int lineThickness = *mThickness ? args::get(*mThickness) : 1;
    const FlattenHeightMode resolvedMode = ResolveMode_(mode, hasHeight);

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
        .referenceTileX = tileX1,
        .referenceTileZ = tileZ1,
        .mode = resolvedMode,
        .shape = ResolveShape_(shape),
        .explicitHeight = resolvedMode == FlattenHeightMode::Explicit ? heightValue : 250.0f,
        .deltaHeight = resolvedMode == FlattenHeightMode::Delta && hasHeight ? heightValue : 7.5f,
        .lineThickness = lineThickness
    });
}

const char* FlattenTool::GetName() const {
    return "flatten";
}

const char* FlattenTool::GetDescription() const {
    return "Flatten terrain to specified height";
}

const char* FlattenTool::GetUsage() const {
    return "flatten <x1> <z1> [<x2> <z2>] [--height=<value>] [--mode=<avg|refavg|min|max|delta>] [--shape=<rect|line>] [--thickness=<n>]";
}

FlattenHeightMode FlattenTool::ResolveMode_(const std::string& mode, const bool hasHeight) {
    if (hasHeight && mode != "delta") {
        return FlattenHeightMode::Explicit;
    }
    if (mode == "min") {
        return FlattenHeightMode::Minimum;
    }
    if (mode == "max") {
        return FlattenHeightMode::Maximum;
    }
    if (mode == "refavg" || mode == "reference" || mode == "referenceavg") {
        return FlattenHeightMode::ReferenceTileAverage;
    }
    if (mode == "delta") {
        return FlattenHeightMode::Delta;
    }
    return FlattenHeightMode::Average;
}

FlattenShapeMode FlattenTool::ResolveShape_(const std::string& shape) {
    if (shape == "line" || shape == "linemask" || shape == "mask" || shape == "diag" || shape == "diagonal") {
        return FlattenShapeMode::LineMask;
    }
    return FlattenShapeMode::Rectangle;
}
