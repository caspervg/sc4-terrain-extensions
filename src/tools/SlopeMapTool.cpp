#include "SlopeMapTool.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "args.hxx"
#include "utils/Logger.h"
#include "viz/TerrainSlopeRenderer.hpp"

namespace {
std::string ModeName(const SlopeRenderMode mode) {
    return mode == SlopeRenderMode::PlaneGrade ? "plane-grade" : "max-edge-grade";
}
}

SlopeMapTool::SlopeMapTool(cISTETerrain* terrain, TerrainSlopeRenderer& renderer)
    : terrain_(terrain)
    , renderer_(renderer) {
}

SlopeMapTool::CommandResult SlopeMapTool::ExecuteCommand(const std::vector<std::string>& tokens) {
    if (!terrain_) {
        return {false, "Slope error", "Terrain is not available."};
    }
    if (tokens.empty()) {
        return {false, "Slope error", "No slope command was provided."};
    }
    if (tokens.size() == 1) {
        const bool enableNow = !renderer_.IsEnabled();
        renderer_.SetEnabled(enableNow, terrain_);
        LOG_INFO("Slope heatmap {}", enableNow ? "enabled" : "disabled");
        return {};
    }
    if (ToLower_(tokens[1]) == "help") {
        return {true, "Slope help", BuildHelpText_()};
    }

    args::ArgumentParser parser("Slope overlay commands");
    args::HelpFlag help(parser, "help", "Show slope help", {"help"});
    args::Command setCommand(parser, "set", "Update slope overlay settings");
    args::Command statusCommand(parser, "status", "Show current slope settings");
    args::Command refreshCommand(parser, "refresh", "Rebuild the slope overlay");
    args::Command resetCommand(parser, "reset", "Restore default slope settings");

    args::ValueFlag<std::string> mode(setCommand, "mode",
        "Slope calculation mode (max-edge-grade|plane-grade)", args::Matcher{"mode"});
    args::ValueFlag<float> opacity(setCommand, "alpha",
        "Overlay opacity in the range 0..1", args::Matcher{"opacity"});
    args::ValueFlag<float> minGrade(setCommand, "grade",
        "Hide tiles below this grade percentage", args::Matcher{"min-grade"});
    args::ValueFlag<std::string> thresholds(setCommand, "values",
        "Band thresholds as a,b,c,d in ascending percent grade", args::Matcher{"thresholds"});

    std::vector<char*> argv;
    argv.reserve(tokens.size());
    for (const auto& token : tokens) {
        argv.push_back(const_cast<char*>(token.c_str()));
    }

    try {
        parser.ParseCLI(static_cast<int>(argv.size()), argv.data());
    } catch (args::Help&) {
        return {true, "Slope help", BuildHelpText_()};
    } catch (args::ParseError& e) {
        return {false, "Slope parse error", std::string("Parse error: ") + e.what() + "\n\n" + BuildHelpText_()};
    } catch (args::ValidationError& e) {
        return {false, "Slope validation error", std::string("Validation error: ") + e.what() + "\n\n" + BuildHelpText_()};
    }

    if (statusCommand) {
        return {true, "Slope status", BuildStatusText_()};
    }

    if (refreshCommand) {
        if (renderer_.IsEnabled()) {
            renderer_.Rebuild(terrain_);
            LOG_INFO("Slope heatmap refreshed");
        } else {
            LOG_INFO("Slope refresh skipped because the overlay is OFF");
        }
        return {};
    }

    if (resetCommand) {
        renderer_.ResetConfig(terrain_, true);
        LOG_INFO("Slope settings reset to defaults");
        return {true, "Slope status", BuildStatusText_()};
    }

    if (setCommand) {
        const bool hasAnySetting = mode || opacity || minGrade || thresholds;
        if (!hasAnySetting) {
            return {false, "Slope validation error", "No settings were provided.\n\n" + BuildHelpText_()};
        }

        SlopeRenderConfig config = renderer_.GetConfig();
        if (mode) {
            const std::string value = ToLower_(args::get(mode));
            if (value == "max-edge-grade" || value == "max-edge" || value == "edge") {
                config.mode = SlopeRenderMode::MaxEdgeGrade;
            } else if (value == "plane-grade" || value == "plane") {
                config.mode = SlopeRenderMode::PlaneGrade;
            } else {
                return {false, "Slope validation error",
                    "Invalid --mode value. Expected max-edge-grade or plane-grade."};
            }
        }
        if (opacity) {
            config.opacity = args::get(opacity);
        }
        if (minGrade) {
            config.minGrade = args::get(minGrade);
        }
        if (thresholds) {
            std::array<float, 4> parsed{};
            if (!TryParseThresholds_(args::get(thresholds), parsed)) {
                return {false, "Slope validation error",
                    "Invalid --thresholds value. Expected four comma-separated ascending numbers, for example 3,7,12,20."};
            }
            config.thresholds = parsed;
        }

        std::string validationError;
        if (!renderer_.SetConfig(config, terrain_, true, &validationError)) {
            return {false, "Slope validation error", validationError};
        }

        LOG_INFO("Slope settings updated: mode={} opacity={} min-grade={} thresholds={},{},{},{}",
            ModeName(config.mode),
            config.opacity,
            config.minGrade,
            config.thresholds[0],
            config.thresholds[1],
            config.thresholds[2],
            config.thresholds[3]);

        return {true, "Slope status", BuildStatusText_()};
    }

    return {false, "Slope error", BuildHelpText_()};
}

void SlopeMapTool::SetTerrain(cISTETerrain* terrain) {
    terrain_ = terrain;
}

std::string SlopeMapTool::BuildHelpText_() const {
    return
        "slope\n"
        "  Toggle the slope heatmap on or off.\n"
        "  Example: slope\n\n"
        "slope set [options]\n"
        "  Update slope heatmap settings without toggling.\n"
        "  If the overlay is ON, settings are applied immediately.\n"
        "  Example: slope set --mode=plane-grade --opacity=0.65\n\n"
        "slope status\n"
        "  Show the current slope settings.\n"
        "  Example: slope status\n\n"
        "slope refresh\n"
        "  Rebuild the slope heatmap using the current settings.\n"
        "  Example: slope refresh\n\n"
        "slope reset\n"
        "  Restore default slope settings.\n"
        "  Example: slope reset\n\n"
        "Options for slope set:\n"
        "  --mode=<max-edge-grade|plane-grade>\n"
        "      Choose how slope is computed for each tile.\n"
        "  --opacity=<0..1>\n"
        "      Overlay opacity. 0 is invisible, 1 is fully opaque.\n"
        "  --min-grade=<percent>\n"
        "      Hide tiles below this percent grade.\n"
        "  --thresholds=<a,b,c,d>\n"
        "      Four ascending grade thresholds used for color bands.\n\n"
        "Examples:\n"
        "  slope set --mode=max-edge-grade\n"
        "  slope set --mode=plane-grade --opacity=0.65\n"
        "  slope set --min-grade=5 --thresholds=4,10,20,35\n";
}

std::string SlopeMapTool::BuildStatusText_() const {
    const SlopeRenderConfig& config = renderer_.GetConfig();

    std::ostringstream oss;
    oss << "Slope heatmap: " << (renderer_.IsEnabled() ? "ON" : "OFF") << '\n'
        << "mode=" << ModeName(config.mode) << '\n'
        << "opacity=" << FormatFloat_(config.opacity) << '\n'
        << "min-grade=" << FormatFloat_(config.minGrade) << "%\n"
        << "thresholds="
        << FormatFloat_(config.thresholds[0]) << ','
        << FormatFloat_(config.thresholds[1]) << ','
        << FormatFloat_(config.thresholds[2]) << ','
        << FormatFloat_(config.thresholds[3]) << '%';
    return oss.str();
}

std::string SlopeMapTool::FormatFloat_(const float value, const int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string text = oss.str();

    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }

    return text;
}

std::string SlopeMapTool::ToLower_(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool SlopeMapTool::TryParseThresholds_(const std::string& text, std::array<float, 4>& outValues) {
    std::stringstream ss(text);
    std::string part;
    for (size_t i = 0; i < outValues.size(); ++i) {
        if (!std::getline(ss, part, ',')) {
            return false;
        }
        try {
            outValues[i] = std::stof(part);
        } catch (...) {
            return false;
        }
    }
    if (std::getline(ss, part, ',')) {
        return false;
    }
    return true;
}
