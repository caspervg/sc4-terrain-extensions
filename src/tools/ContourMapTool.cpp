#include "ContourMapTool.hpp"

#include "utils/Logger.h"
#include "viz/TerrainContourRenderer.hpp"

ContourMapTool::ContourMapTool(cISTETerrain* terrain, TerrainContourRenderer& renderer)
    : TerrainTool(terrain)
    , renderer_(renderer) {
}

void ContourMapTool::RegisterArguments(args::Group& commands) {
    mCommand = std::make_unique<args::Command>(commands, "contour",
        "Control full-map contour overlay");
    mOn = std::make_unique<args::Flag>(*mCommand, "on", "Enable contour overlay", args::Matcher{"on"});
    mOff = std::make_unique<args::Flag>(*mCommand, "off", "Disable contour overlay", args::Matcher{"off"});
    mToggle = std::make_unique<args::Flag>(*mCommand, "toggle", "Toggle contour overlay", args::Matcher{"toggle"});
    mIntervalMeters = std::make_unique<args::ValueFlag<float>>(*mCommand, "meters",
        "Contour interval in meters", args::Matcher{"interval"}, 5.0f);
    mMajorEvery = std::make_unique<args::ValueFlag<int>>(*mCommand, "count",
        "Draw a major contour every N lines", args::Matcher{"major-every"}, 5);
    mRefresh = std::make_unique<args::Flag>(*mCommand, "refresh", "Force rebuild while enabled", args::Matcher{"refresh"});
}

bool ContourMapTool::ShouldExecute(const args::ArgumentParser& parser) const {
    return *mCommand;
}

void ContourMapTool::Execute(const args::ArgumentParser& parser) {
    if (!terrain_) {
        LOG_ERROR("ContourMapTool: terrain is not available");
        return;
    }

    const bool hadOnOffToggleIntent = *mOn || *mOff || *mToggle;
    const bool hadParamIntent = *mIntervalMeters || *mMajorEvery || *mRefresh;

    const bool wasEnabled = renderer_.IsEnabled();
    bool enableNow = wasEnabled;

    if (*mOn) {
        enableNow = true;
    }
    if (*mOff) {
        enableNow = false;
    }
    if (*mToggle) {
        enableNow = !enableNow;
    }

    // No explicit action means "toggle", for quick use.
    if (!hadOnOffToggleIntent && !hadParamIntent) {
        enableNow = !enableNow;
    }

    bool paramsChanged = false;

    if (*mIntervalMeters) {
        const float interval = args::get(*mIntervalMeters);
        if (interval <= 0.0f) {
            LOG_ERROR("ContourMapTool: --interval must be > 0");
            return;
        }
        renderer_.SetIntervalMeters(interval);
        paramsChanged = true;
    }

    if (*mMajorEvery) {
        const int majorEvery = args::get(*mMajorEvery);
        if (majorEvery < 1) {
            LOG_ERROR("ContourMapTool: --major-every must be >= 1");
            return;
        }
        renderer_.SetMajorEvery(majorEvery);
        paramsChanged = true;
    }

    if (enableNow != wasEnabled) {
        renderer_.SetEnabled(enableNow, terrain_);
    } else if (enableNow && (paramsChanged || *mRefresh)) {
        renderer_.Rebuild(terrain_);
    }

    LOG_INFO("Contour map: {} | interval={}m | major-every={}",
        renderer_.IsEnabled() ? "ON" : "OFF",
        renderer_.GetIntervalMeters(),
        renderer_.GetMajorEvery());
}

const char* ContourMapTool::GetName() const {
    return "contour";
}

const char* ContourMapTool::GetDescription() const {
    return "Control full-map contour overlay";
}

const char* ContourMapTool::GetUsage() const {
    return "contour [--on|--off|--toggle] [--interval=<meters>] [--major-every=<N>] [--refresh]";
}
